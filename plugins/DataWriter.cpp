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
#include "dfmodules/datawriterinfo/InfoNljs.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "daqdataformats/Fragment.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "logging/Logging.hpp"
#include "rcif/cmd/Nljs.hpp"

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
  TLVL_SEQNO_MAP_CONTENTS = 13,
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
  auto qi = appfwk::queue_index(init_data, { "trigger_record_input_queue", "token_output_queue" });
  try {
    m_trigger_record_input_queue.reset(new trigrecsource_t(qi["trigger_record_input_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_record_input_queue", excpt);
  }

  try {
    m_trigger_decision_token_output_queue.reset(new tokensink_t(qi["token_output_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "token_output_queue", excpt);
  }
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DataWriter::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  datawriterinfo::Info dwi;

  dwi.records_received = m_records_received_tot.load();
  dwi.new_records_received = m_records_received.exchange(0);
  dwi.records_written = m_records_written_tot.load();
  dwi.new_records_written = m_records_written.exchange(0);
  dwi.bytes_output = m_bytes_output.load();

  ci.add(dwi);
}
void
DataWriter::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  datawriter::ConfParams conf_params = payload.get<datawriter::ConfParams>();
  m_initial_tokens = conf_params.initial_token_count;
  m_data_storage_prescale = conf_params.data_storage_prescale;
  TLOG_DEBUG(TLVL_CONFIG) << get_name() << ": initial_token_count is " << conf_params.initial_token_count;
  TLOG_DEBUG(TLVL_CONFIG) << get_name() << ": data_storage_prescale is " << m_data_storage_prescale;
  TLOG_DEBUG(TLVL_CONFIG) << get_name() << ": data_store_parameters are " << conf_params.data_store_parameters;
  m_min_write_retry_time_usec = conf_params.min_write_retry_time_usec;
  if (m_min_write_retry_time_usec < 1) {
    m_min_write_retry_time_usec = 1;
  }
  m_max_write_retry_time_usec = conf_params.max_write_retry_time_usec;
  m_write_retry_time_increase_factor = conf_params.write_retry_time_increase_factor;

  // create the DataStore instance here
  try {
    m_data_writer = make_data_store(payload["data_store_parameters"]);
  } catch (const ers::Issue& excpt) {
    throw UnableToConfigure(ERS_HERE, get_name(), excpt);
  }

  // ensure that we have a valid dataWriter instance
  if (m_data_writer.get() == nullptr) {
    throw InvalidDataWriter(ERS_HERE, get_name());
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
DataWriter::do_start(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  rcif::cmd::StartParams start_params = payload.get<rcif::cmd::StartParams>();
  m_data_storage_is_enabled = (!start_params.disable_data_storage);
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

  m_seqno_counts.clear();
  for (int ii = 0; ii < m_initial_tokens; ++ii) {
    dfmessages::TriggerDecisionToken token;
    token.run_number = m_run_number;
    m_trigger_decision_token_output_queue->push(std::move(token));
  }

  m_thread.start_working_thread(get_name());

  TLOG() << get_name() << " successfully started for run number " << m_run_number;
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DataWriter::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  m_thread.stop_working_thread();

  // 04-Feb-2021, KAB: added this call to allow DataStore to finish up with this run.
  // I've put this call fairly late in this method so that any draining of queues
  // (or whatever) can take place before we finalize things in the DataStore.
  if (m_data_storage_is_enabled) {
    try {
      m_data_writer->finish_with_run(m_run_number);
    } catch (const std::exception& excpt) {
      ers::error(ProblemDuringStop(ERS_HERE, get_name(), m_run_number, excpt));
    }
  }

  TLOG() << get_name() << " successfully stopped for run number " << m_run_number;
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
  m_records_received = 0;
  m_records_received_tot = 0;
  m_records_written = 0;
  m_records_written_tot = 0;
  m_bytes_output = 0;

  // ensure that we have a valid dataWriter instance
  if (m_data_writer.get() == nullptr) {
    // this check is done essentially to notify the user
    // in case the "start" has been called before the "conf"
    ers::fatal(InvalidDataWriter(ERS_HERE, get_name()));
  }

  while (running_flag.load() || m_trigger_record_input_queue->can_pop()) {

    std::unique_ptr<daqdataformats::TriggerRecord> trigger_record_ptr;

    // receive the next TriggerRecord
    try {
      m_trigger_record_input_queue->pop(trigger_record_ptr, m_queue_timeout);
      ++m_records_received;
      ++m_records_received_tot;
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Popped the TriggerRecord for trigger number "
                                  << trigger_record_ptr->get_header_ref().get_trigger_number() << "."
                                  << trigger_record_ptr->get_header_ref().get_sequence_number()
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
    if (m_data_storage_prescale <= 1 || ((m_records_received_tot.load() % m_data_storage_prescale) == 1)) {
      std::vector<KeyedDataBlock> data_block_list;
      uint64_t bytes_in_data_blocks = 0; // NOLINT(build/unsigned)

      // First deal with the trigger record header
      const void* trh_ptr = trigger_record_ptr->get_header_ref().get_storage_location();
      size_t trh_size = trigger_record_ptr->get_header_ref().get_total_size_bytes();
      // Apa number and link number in trh_key
      // are not taken into account for the Trigger
      StorageKey trh_key(trigger_record_ptr->get_header_ref().get_run_number(),
                         trigger_record_ptr->get_header_ref().get_trigger_number(),
                         StorageKey::DataRecordGroupType::kTriggerRecordHeader,
                         1,
                         1);
      trh_key.m_this_sequence_number = trigger_record_ptr->get_header_ref().get_sequence_number();
      trh_key.m_max_sequence_number = trigger_record_ptr->get_header_ref().get_max_sequence_number();
      KeyedDataBlock trh_block(trh_key);
      trh_block.m_unowned_data_start = trh_ptr;
      trh_block.m_data_size = trh_size;
      bytes_in_data_blocks += trh_block.m_data_size;
      data_block_list.emplace_back(std::move(trh_block));

      // Loop over the fragments
      const auto& frag_vec = trigger_record_ptr->get_fragments_ref();
      for (const auto& frag_ptr : frag_vec) {

        // print out some debug information, if requested
        TLOG_DEBUG(TLVL_FRAGMENT_HEADER_DUMP)
          << get_name() << ": Partial(?) contents of the Fragment from link " << frag_ptr->get_element_id();
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
        StorageKey::DataRecordGroupType group_type =
          get_group_type(frag_ptr->get_element_id().system_type, frag_ptr->get_fragment_type());
        StorageKey fragment_skey(frag_ptr->get_run_number(),
                                 frag_ptr->get_trigger_number(),
                                 group_type,
                                 frag_ptr->get_element_id().region_id,
                                 frag_ptr->get_element_id().element_id);
        fragment_skey.m_this_sequence_number = frag_ptr->get_sequence_number();
        fragment_skey.m_max_sequence_number = trigger_record_ptr->get_header_ref().get_max_sequence_number();
        KeyedDataBlock data_block(fragment_skey);
        data_block.m_unowned_data_start = frag_ptr->get_storage_location();
        data_block.m_data_size = frag_ptr->get_size();
        bytes_in_data_blocks += data_block.m_data_size;

        data_block_list.emplace_back(std::move(data_block));
      }

      // write the TRH and the fragments as a set of data blocks
      if (m_data_storage_is_enabled) {

        bool should_retry = true;
        size_t retry_wait_usec = m_min_write_retry_time_usec;
        do {
          should_retry = false;
          try {
            m_data_writer->write(data_block_list);
            ++m_records_written;
            ++m_records_written_tot;
            m_bytes_output += bytes_in_data_blocks;
          } catch (const RetryableDataStoreProblem& excpt) {
            should_retry = true;
            ers::error(DataWritingProblem(ERS_HERE,
                                          get_name(),
                                          trigger_record_ptr->get_header_ref().get_trigger_number(),
                                          trigger_record_ptr->get_header_ref().get_run_number(),
                                          excpt));
            if (retry_wait_usec > m_max_write_retry_time_usec) {
              retry_wait_usec = m_max_write_retry_time_usec;
            }
            usleep(retry_wait_usec);
            retry_wait_usec *= m_write_retry_time_increase_factor;
          } catch (const std::exception& excpt) {
            ers::error(DataWritingProblem(ERS_HERE,
                                          get_name(),
                                          trigger_record_ptr->get_header_ref().get_trigger_number(),
                                          trigger_record_ptr->get_header_ref().get_run_number(),
                                          excpt));
          }
        } while (should_retry && running_flag.load());
      }
    }

    bool send_trigger_complete_message = true;
    if (trigger_record_ptr->get_header_ref().get_max_sequence_number() > 0) {
      send_trigger_complete_message = false;
      dataformats::trigger_number_t trigno = trigger_record_ptr->get_header_ref().get_trigger_number();
      if (m_seqno_counts.count(trigno) > 0) {
        ++m_seqno_counts[trigno];
      } else {
        m_seqno_counts[trigno] = 1;
      }
      // in the following comparison GT (>) is used since the counts are one-based and the
      // max sequence number is zero-based.
      if (m_seqno_counts[trigno] > trigger_record_ptr->get_header_ref().get_max_sequence_number()) {
        send_trigger_complete_message = true;
        m_seqno_counts.erase(trigno);
      } else {
        // by putting this TLOG call in an "else" clause, we avoid resurrecting the "trigno"
        // entry in the map after erasing it above. In other words, if we move this TLOG outside
        // the "else" clause, the map will forever increase in size.
        TLOG_DEBUG(TLVL_SEQNO_MAP_CONTENTS) << get_name() << ": the sequence number count for trigger number " << trigno
                                            << " is " << m_seqno_counts[trigno] << " (number of entries "
                                            << "in the seqno map is " << m_seqno_counts.size() << ").";
      }
    }
    if (send_trigger_complete_message) {
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Pushing the TriggerDecisionToken for trigger number "
                                  << trigger_record_ptr->get_header_ref().get_trigger_number()
                                  << " onto the relevant output queue";
      dfmessages::TriggerDecisionToken token;
      token.run_number = m_run_number;
      token.trigger_number = trigger_record_ptr->get_header_ref().get_trigger_number();
      m_trigger_decision_token_output_queue->push(std::move(token));
    }
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
} // NOLINT(readability/fn_size)

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DataWriter)
