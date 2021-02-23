/**
 * @file DataWriter.cpp DataWriter class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DataWriter.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/KeyedDataBlock.hpp"
#include "dfmodules/StorageKey.hpp"
#include "dfmodules/datawriter/Nljs.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "dataformats/Fragment.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/TriggerInhibit.hpp"
#include "logging/Logging.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
//#define TRACE_NAME "DataWriter"                   // NOLINT This is the default
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10,
  TLVL_FRAGMENT_HEADER_DUMP = 17
};

namespace dunedaq {
namespace dfmodules {

DataWriter::DataWriter(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_thread(std::bind(&DataWriter::do_work, this, std::placeholders::_1))
  , m_queue_timeout(100)
  , m_data_storage_is_enabled(true)
  , m_trigger_record_input_queue(nullptr)
  , m_trigger_decision_token_output_queue(nullptr)
{
  register_command("conf", &DataWriter::do_conf);
  register_command("start", &DataWriter::do_start);
  register_command("stop", &DataWriter::do_stop);
  register_command("scrap", &DataWriter::do_scrap);
}

void
DataWriter::init(const data_t& init_data)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto qi = appfwk::queue_index(
    init_data, { "trigger_record_input_queue", "trigger_decision_for_inhibit", "trigger_inhibit_output_queue" });
  try {
    m_trigger_record_input_queue.reset(new trigrecsource_t(qi["trigger_record_input_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_record_input_queue", excpt);
  }

  using trigdecsource_t = dunedaq::appfwk::DAQSource<dfmessages::TriggerDecision>;
  std::unique_ptr<trigdecsource_t> trig_dec_queue_for_inh;
  try {
    trig_dec_queue_for_inh.reset(new trigdecsource_t(qi["trigger_decision_for_inhibit"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_for_inhibit", excpt);
  }
  using triginhsink_t = dunedaq::appfwk::DAQSink<dfmessages::TriggerInhibit>;
  std::unique_ptr<triginhsink_t> trig_inh_output_queue;
  try {
    trig_inh_output_queue.reset(new triginhsink_t(qi["trigger_inhibit_output_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_inhibit_output_queue", excpt);
  }
  m_trigger_inhibit_agent.reset(
    new TriggerInhibitAgent(get_name(), std::move(trig_dec_queue_for_inh), std::move(trig_inh_output_queue)));

  try {
    m_trigger_decision_token_output_queue.reset(new tokensink_t(qi["token_output_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "token_output_queue", excpt);
  }
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DataWriter::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  datawriter::ConfParams conf_params = payload.get<datawriter::ConfParams>();
  m_trigger_inhibit_agent->set_threshold_for_inhibit(conf_params.threshold_for_inhibit);
  m_initial_tokens = conf_params.threshold_for_inhibit; // TODO ELF 2/17/2021: Maybe use a different parameter?
  TLOG_DEBUG(TLVL_CONFIG) << get_name() << ": threshold_for_inhibit is " << conf_params.threshold_for_inhibit;
  TLOG_DEBUG(TLVL_CONFIG) << get_name() << ": data_store_parameters are " << conf_params.data_store_parameters;

  // create the DataStore instance here
  m_data_writer = make_data_store(payload["data_store_parameters"]);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
DataWriter::do_start(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  datawriter::StartParams start_params = payload.get<datawriter::StartParams>();
  m_data_storage_is_enabled = (!start_params.disable_data_storage);
  m_data_storage_prescale = start_params.data_storage_prescale;
  m_run_number = start_params.run;

  // 04-Feb-2021, KAB: added this call to allow DataStore to prepare for the run.
  // I've put this call fairly early in this method because it could throw an
  // exception and abort the run start.  And, it seems sensible to avoid starting
  // threads, etc. if we throw an exception.
  if (m_data_storage_is_enabled) {
    try {
      m_data_writer->prepare_for_run(m_run_number);
    } catch (const ers::Issue& excpt) {
      throw UnableToStart(ERS_HERE, get_name(), m_run_number, excpt);
    }
  }

  if (m_trigger_decision_token_output_queue != nullptr) {
    for (int ii = 0; ii < m_initial_tokens; ++ii) {
      dfmessages::TriggerDecisionToken token;
      token.run_number = m_run_number;
      m_trigger_decision_token_output_queue->push(std::move(token));
    }
  }

  m_trigger_inhibit_agent->start_checking();
  m_thread.start_working_thread();

  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DataWriter::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  m_trigger_inhibit_agent->stop_checking();
  m_thread.stop_working_thread();

  // 04-Feb-2021, KAB: added this call to allow DataStore to finish up with this run.
  // I've put this call fairly late in this method so that any draining of queues
  // (or whatever) can take place before we finalize things in the DataStore.
  if (m_data_storage_is_enabled) {
    m_data_writer->finish_with_run(m_run_number);
  }

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DataWriter::do_scrap(const data_t& /*payload*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  // clear/reset the DataStore instance here
  m_data_writer.reset();

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
DataWriter::do_work(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int32_t received_count = 0;
  int32_t written_count = 0;

  // ensure that we have a valid dataWriter instance
  if (m_data_writer.get() == nullptr) {
    throw InvalidDataWriterError(ERS_HERE, get_name());
  }

  std::chrono::steady_clock::time_point progress_report_time = std::chrono::steady_clock::now();
  while (running_flag.load() || m_trigger_record_input_queue->can_pop()) {
    std::unique_ptr<dataformats::TriggerRecord> trigger_record_ptr;

    // receive the next TriggerRecord
    try {
      m_trigger_record_input_queue->pop(trigger_record_ptr, m_queue_timeout);
      ++received_count;
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Popped the TriggerRecord for trigger number "
                                  << trigger_record_ptr->get_header_ref().get_trigger_number()
                                  << " off the input queue";
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      // it is perfectly reasonable that there might be no data in the queue
      // some fraction of the times that we check, so we just continue on and try again
      continue;
    }

    // 03-Feb-2021, KAB: adding support for a data-storage prescale.
    // In this "if" statement, I deliberately compare the result of (N mod prescale) to 1
    // instead of zero, since I think that it would be nice to always get the first event
    // written out.
    if (m_data_storage_prescale <= 1 || ((received_count % m_data_storage_prescale) == 1)) {
      std::vector<KeyedDataBlock> data_block_list;

      // First deal with the trigger record header
      const void* trh_ptr = trigger_record_ptr->get_header_ref().get_storage_location();
      size_t trh_size = trigger_record_ptr->get_header_ref().get_total_size_bytes();
      // Apa number and link number in trh_key
      // are not taken into account for the Trigger
      StorageKey trh_key(trigger_record_ptr->get_header_ref().get_run_number(),
                         trigger_record_ptr->get_header_ref().get_trigger_number(),
                         "TriggerRecordHeader",
                         1,
                         1);
      KeyedDataBlock trh_block(trh_key);
      trh_block.m_unowned_data_start = trh_ptr;
      trh_block.m_data_size = trh_size;
      data_block_list.emplace_back(std::move(trh_block));

      // Loop over the fragments
      const auto& frag_vec = trigger_record_ptr->get_fragments_ref();
      for (const auto& frag_ptr : frag_vec) {

        // print out some debug information, if requested
        TLOG_DEBUG(TLVL_FRAGMENT_HEADER_DUMP)
          << get_name() << ": Partial(?) contents of the Fragment from link " << frag_ptr->get_link_id().m_link_number;
        const size_t number_of_32bit_values_per_row = 5;
        const size_t max_number_of_rows = 5;
        int number_of_32bit_values_to_print =
          std::min((number_of_32bit_values_per_row * max_number_of_rows),
                   (static_cast<size_t>(frag_ptr->get_size()) / sizeof(uint32_t)));               // NOLINT
        const uint32_t* mem_ptr = static_cast<const uint32_t*>(frag_ptr->get_storage_location()); // NOLINT
        std::ostringstream oss_hexdump;
        for (int idx = 0; idx < number_of_32bit_values_to_print; ++idx) {
          if ((idx % number_of_32bit_values_per_row) == 0) {
            oss_hexdump << "32-bit offset " << std::setw(2) << std::setfill(' ') << idx << ":" << std::hex;
          }
          oss_hexdump << " 0x" << std::setw(8) << std::setfill('0') << *mem_ptr;
          ++mem_ptr;
          if (((idx + 1) % number_of_32bit_values_per_row) == 0) {
            oss_hexdump << std::dec;
            TLOG_DEBUG(TLVL_FRAGMENT_HEADER_DUMP) << get_name() << ": " << oss_hexdump.str();
            oss_hexdump.str("");
            oss_hexdump.clear();
          }
        }
        if (oss_hexdump.str().length() > 0) {
          TLOG_DEBUG(TLVL_FRAGMENT_HEADER_DUMP) << get_name() << ": " << oss_hexdump.str();
        }

        // add information about each Fragment to the list of data blocks to be stored
        // //StorageKey fragment_skey(trigger_record_ptr->get_run_number(), trigger_record_ptr->get_trigger_number,
        // "TPC",
        StorageKey fragment_skey(frag_ptr->get_run_number(),
                                 frag_ptr->get_trigger_number(),
                                 "TPC",
                                 frag_ptr->get_link_id().m_apa_number,
                                 frag_ptr->get_link_id().m_link_number);
        KeyedDataBlock data_block(fragment_skey);
        data_block.m_unowned_data_start = frag_ptr->get_storage_location();
        data_block.m_data_size = frag_ptr->get_size();

        data_block_list.emplace_back(std::move(data_block));
      }

      // write the TRH and the fragments as a set of data blocks
      if (m_data_storage_is_enabled) {
        m_data_writer->write(data_block_list);
        ++written_count;
      }
    }

    // progress updates
    std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();
    if (elapsed_seconds(progress_report_time, current_time) >= 3.0) {
      progress_report_time = current_time;
      std::ostringstream oss_prog;
      oss_prog << ": Processing trigger number " << trigger_record_ptr->get_header_ref().get_trigger_number()
               << ", this is one of " << received_count << " trigger records received so far. " << written_count
               << " trigger records have been written to the data store.";
      TLOG() << ProgressUpdate(ERS_HERE, get_name(), oss_prog.str());
    }

    // tell the TriggerInhibitAgent the trigger_number of this TriggerRecord so that
    // it can check whether an Inhibit needs to be asserted or cleared.
    m_trigger_inhibit_agent->set_latest_trigger_number(trigger_record_ptr->get_header_ref().get_trigger_number());
    if (m_trigger_decision_token_output_queue != nullptr) {
      dfmessages::TriggerDecisionToken token;
      token.run_number = m_run_number;
      token.trigger_number = trigger_record_ptr->get_header_ref().get_trigger_number();
      m_trigger_decision_token_output_queue->push(std::move(token));
    }
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, received trigger record messages for " << received_count << " triggers.";
  TLOG() << ProgressUpdate(ERS_HERE, get_name(), oss_summ.str());
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DataWriter)
