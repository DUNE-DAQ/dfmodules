/**
 * @file FilterWriterModule.cpp FilterWriterModule class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FilterWriterModule.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/opmon/DataWriter.pb.h"

#include "appmodel/DataStoreConf.hpp"
#include "appmodel/FilterWriterModule.hpp"
#include "appmodel/TRBModule.hpp"
#include "confmodel/Application.hpp"
#include "confmodel/Connection.hpp"
#include "confmodel/Session.hpp"
#include "daqdataformats/Fragment.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/TriggerRecord_serialization.hpp"
#include "iomanager/IOManager.hpp"
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
// #define TRACE_NAME "FilterWriterModule"                   // NOLINT This is the default
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

FilterWriterModule::FilterWriterModule(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_queue_timeout(100)
  , m_thread(std::bind(&FilterWriterModule::do_work, this, std::placeholders::_1))
{
  register_command("conf", &FilterWriterModule::do_conf);
  register_command("start", &FilterWriterModule::do_start);
  register_command("stop", &FilterWriterModule::do_stop);
  register_command("scrap", &FilterWriterModule::do_scrap);
}

void
FilterWriterModule::init(std::shared_ptr<appfwk::ConfigurationManager> mcfg)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto mdal = mcfg->get_dal<appmodel::FilterWriterModule>(get_name());
  if (!mdal) {
    throw appfwk::CommandFailed(ERS_HERE, "init", get_name(), "Unable to retrieve configuration object");
  }
  auto iom = iomanager::IOManager::get();

  auto inputs = mdal->get_inputs();
  auto outputs = mdal->get_outputs();

  if (inputs.size() != 1) {
    throw appfwk::CommandFailed(ERS_HERE, "init", get_name(), "Expected 1 input, got " + std::to_string(inputs.size()));
  }
  if (outputs.size() != 1) {
    throw appfwk::CommandFailed(
      ERS_HERE, "init", get_name(), "Expected 1 output, got " + std::to_string(outputs.size()));
  }

  m_module_configuration = mcfg;
  m_filter_writer_conf = mdal->get_configuration();
  m_writer_identifier = mdal->get_writer_identifier();

  if (inputs[0]->get_data_type() != datatype_to_string<std::unique_ptr<daqdataformats::TriggerRecord>>()) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "TriggerRecord Input queue");
  }

  m_trigger_record_connection = inputs[0]->UID();

  // try to create the receiver to see test the connection anyway
  m_tr_receiver = iom->get_receiver<std::unique_ptr<daqdataformats::TriggerRecord>>(m_trigger_record_connection);


  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
FilterWriterModule::generate_opmon_data()
{

  opmon::DataWriterInfo dwi;

  dwi.set_records_received(m_records_received_tot.load());
  //   dwi.new_records_received = m_records_received.exchange(0);
  dwi.set_records_written(m_records_written_tot.load());
  dwi.set_new_records_written(m_records_written.exchange(0));
  //   dwi.bytes_output = m_bytes_output_tot.load();  MR: byte writing to be delegated to DataStorage
  //   dwi.new_bytes_output = m_bytes_output.exchange(0);
  dwi.set_writing_time_us(m_writing_us.exchange(0));

  publish(std::move(dwi));
}

void
FilterWriterModule::do_conf(const CommandData_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  TLOG_DEBUG(TLVL_CONFIG) << get_name() << ": data_store_parameters are "
                          << m_filter_writer_conf->get_data_store_params();
  m_min_write_retry_time_usec = m_filter_writer_conf->get_min_write_retry_time_ms() * 1000;
  if (m_min_write_retry_time_usec < 1) {
    m_min_write_retry_time_usec = 1;
  }
  m_max_write_retry_time_usec = m_filter_writer_conf->get_max_write_retry_time_ms() * 1000;
  m_write_retry_time_increase_factor = m_filter_writer_conf->get_write_retry_time_increase_factor();

  // create the DataStore instance here
  try {
    m_data_writer = make_data_store(m_filter_writer_conf->get_data_store_params()->get_type(),
                                    m_filter_writer_conf->get_data_store_params()->UID(),
                                    m_module_configuration,
                                    m_writer_identifier);
    register_node("data_writer", m_data_writer);
  } catch (const ers::Issue& excpt) {
    throw UnableToConfigure(ERS_HERE, get_name(), excpt);
  }

  m_client = std::make_shared<FilterOrchestratorClient>(m_filter_writer_conf->get_orchestrator_hostname(),
                                                        m_filter_writer_conf->get_orchestrator_port());

  // ensure that we have a valid dataWriter instance
  if (m_data_writer.get() == nullptr) {
    throw InvalidFilterWriterModule(ERS_HERE, get_name());
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
FilterWriterModule::do_start(const CommandData_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  m_run_number = 0;
  m_seqno_counts.clear();

  m_records_received = 0;
  m_records_received_tot = 0;
  m_records_written = 0;
  m_records_written_tot = 0;
  m_bytes_output = 0;
  m_bytes_output_tot = 0;

  m_running.store(true);

  m_thread.start_working_thread(get_name());
  // iomanager::IOManager::get()->add_callback<std::unique_ptr<daqdataformats::TriggerRecord>>(
  // m_trigger_record_connection,
  // bind( &FilterWriterModule::receive_trigger_record, this, std::placeholders::_1) );

  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
FilterWriterModule::do_stop(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  m_running.store(false);
  m_thread.stop_working_thread();
  // iomanager::IOManager::get()->remove_callback<std::unique_ptr<daqdataformats::TriggerRecord>>(
  // m_trigger_record_connection );
  close_run();

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
FilterWriterModule::do_scrap(const CommandData_t& /*payload*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  // clear/reset the DataStore instance here
  m_data_writer.reset();

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
FilterWriterModule::open_run(daqdataformats::run_number_t run_number)
{
  if (run_number == m_run_number)
    return;

  close_run();
  // 04-Feb-2021, KAB: added this call to allow DataStore to prepare for the run.
  // I've put this call fairly early in this method because it could throw an
  // exception and abort the run start.  And, it seems sensible to avoid starting
  // threads, etc. if we throw an exception.
  if (run_number != 0) {
    m_run_number = run_number;
    // ensure that we have a valid dataWriter instance
    if (m_data_writer.get() == nullptr) {
      // this check is done essentially to notify the user
      // in case the "start" has been called before the "conf"
      ers::fatal(InvalidFilterWriterModule(ERS_HERE, get_name()));
    }

    try {
      m_data_writer->prepare_for_run(m_run_number, false);
    } catch (const ers::Issue& excpt) {
      throw UnableToStart(ERS_HERE, get_name(), m_run_number, excpt);
    }
  }
}

void
FilterWriterModule::close_run()
{ // 04-Feb-2021, KAB: added this call to allow DataStore to finish up with this run.
  // I've put this call fairly late in this method so that any draining of queues
  // (or whatever) can take place before we finalize things in the DataStore.
  if (m_run_number != 0) {
    try {
      m_data_writer->finish_with_run(m_run_number);
    } catch (const std::exception& excpt) {
      ers::error(ProblemDuringStop(ERS_HERE, get_name(), m_run_number, excpt));
    }
    m_run_number = 0;
  }
}

void
FilterWriterModule::receive_trigger_record(std::unique_ptr<daqdataformats::TriggerRecord>& trigger_record_ptr)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": receiving a new TR ptr";

  ++m_records_received;
  ++m_records_received_tot;
  TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Obtained the TriggerRecord for trigger number "
                              << trigger_record_ptr->get_header_ref().get_trigger_number() << "."
                              << trigger_record_ptr->get_header_ref().get_sequence_number() << ", run number "
                              << trigger_record_ptr->get_header_ref().get_run_number() << " off the input connection";
  open_run(trigger_record_ptr->get_header_ref().get_run_number());

  std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

  bool should_retry = true;
  size_t retry_wait_usec = m_min_write_retry_time_usec;
  do {
    should_retry = false;
    try {
      m_data_writer->write(*trigger_record_ptr);
      ++m_records_written;
      ++m_records_written_tot;
      m_bytes_output += trigger_record_ptr->get_total_size_bytes();
      m_bytes_output_tot += trigger_record_ptr->get_total_size_bytes();
    } catch (const RetryableDataStoreProblem& excpt) {
      should_retry = true;
      ers::error(DataWritingProblem(ERS_HERE,
                                    get_name(),
                                    trigger_record_ptr->get_header_ref().get_trigger_number(),
                                    trigger_record_ptr->get_header_ref().get_sequence_number(),
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
                                    trigger_record_ptr->get_header_ref().get_sequence_number(),
                                    trigger_record_ptr->get_header_ref().get_run_number(),
                                    excpt));
    }
  } while (should_retry && m_running.load());

  std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now();
  auto writing_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
  m_writing_us += writing_time.count();

  m_client->complete(trigger_record_ptr->get_header_ref().get_run_number(),
                     trigger_record_ptr->get_header_ref().get_trigger_number(),
                     trigger_record_ptr->get_header_ref().get_sequence_number());

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": operations completed for TR";
} // NOLINT(readability/fn_size)

void
FilterWriterModule::do_work(std::atomic<bool>& running_flag)
{
  while (running_flag.load()) {
    try {
      std::unique_ptr<daqdataformats::TriggerRecord> tr = m_tr_receiver->receive(std::chrono::milliseconds(10));
      receive_trigger_record(tr);
    } catch (const iomanager::TimeoutExpired& excpt) {
    } catch (const ers::Issue& excpt) {
      ers::warning(excpt);
    }
  }
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::FilterWriterModule)
