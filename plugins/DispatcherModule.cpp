/**
 * @file DispatcherModule.cpp DispatcherModule class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DispatcherModule.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "appmodel/DispatcherModule.hpp"
#include "confmodel/Connection.hpp"
#include "dfmessages/Fragment_serialization.hpp"
#include "dfmessages/TimeSync.hpp"
#include "iomanager/IOManager.hpp"
#include "logging/Logging.hpp"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "DispatcherModule" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10,
  TLVL_TIME_SYNCS = 12
};

namespace dunedaq {
namespace dfmodules {

DispatcherModule::DispatcherModule(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_tr_thread(std::bind(&DispatcherModule::read_trigger_records, this, std::placeholders::_1))
  , m_ts_thread(std::bind(&DispatcherModule::read_time_slices, this, std::placeholders::_1))
  , m_request_interval(1000)
  , m_queue_timeout(100)
{
  register_command("conf", &DispatcherModule::do_conf);
  register_command("start", &DispatcherModule::do_start);
  register_command("stop", &DispatcherModule::do_stop);
}

void
DispatcherModule::init(std::shared_ptr<appfwk::ConfigurationManager> mcfg)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto mdal = mcfg->get_dal<appmodel::DispatcherModule>(get_name());
  if (!mdal) {
    throw appfwk::CommandFailed(ERS_HERE, "init", get_name(), "Unable to retrieve configuration object");
  }

  m_host = mdal->get_hostname();
  auto outputs = mdal->get_outputs();

  auto iom = iomanager::IOManager::get();
  for (auto con : outputs) {
    if (con->get_data_type() == datatype_to_string<daqdataformats::TriggerRecord>()) {
      m_trigger_record_queue = iom->get_sender<daqdataformats::TriggerRecord>(con->UID());
    }
    if (con->get_data_type() == datatype_to_string<daqdataformats::TimeSlice>()) {
      m_time_slice_queue = iom->get_sender<daqdataformats::TimeSlice>(con->UID());
    }
  }
  m_dispatcher_conf = mdal->get_configuration();
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DispatcherModule::do_conf(const CommandData_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  m_request_interval = std::chrono::milliseconds(m_dispatcher_conf->get_request_interval_ms());
  m_queue_timeout = std::chrono::milliseconds(m_dispatcher_conf->get_queue_timeout_ms());

  m_reader = std::make_shared<HDF5DataReader>(m_host);
  m_client = std::make_shared<FilterOrchestratorClient>(m_dispatcher_conf->get_orchestrator_hostname(),
                                      m_dispatcher_conf->get_orchestrator_port());

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
DispatcherModule::do_start(const CommandData_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  m_sent_trigger_records = 0;
  m_sent_time_slices = 0;

  m_tr_thread.start_working_thread();
  m_ts_thread.start_working_thread();

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DispatcherModule::do_stop(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  m_tr_thread.stop_working_thread();
  m_ts_thread.stop_working_thread();

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

// void
// DispatcherModule::get_info(opmonlib::InfoCollector& ci, int /*level*/)
// {
//   Dispatcherinfo::Info info;
//   info.requests_received = m_received_requests;
//   info.fragments_sent = m_sent_fragments;
//   ci.add(info);
// }

void
DispatcherModule::read_trigger_records(std::atomic<bool>& running_flag)
{
  if (m_trigger_record_queue == nullptr)
    return;
  std::string last_file = "";
  while (running_flag.load()) {
    auto ret = m_client->read_next_triggerrecord(m_host, last_file);
    if (ret) {
      auto tr = m_reader->read_trigger_record(ret->file, ret->trigger_number, ret->sequence_number);
      if (tr) {
        m_trigger_record_queue->send(std::move(*tr), m_queue_timeout);
      }
    }
    std::this_thread::sleep_for(m_request_interval);
  }
}

void
DispatcherModule::read_time_slices(std::atomic<bool>& running_flag)
{
  if (m_time_slice_queue == nullptr)
    return;
  std::string last_file = "";
  while (running_flag.load()) {
    auto ret = m_client->read_next_timeslice(m_host, last_file);
    if (ret) {
      auto ts = m_reader->read_time_slice(ret->file, ret->trigger_number);
      if (ts) {
                  m_time_slice_queue->send(std::move(*ts), m_queue_timeout);
      }
    }
    std::this_thread::sleep_for(m_request_interval);
  }
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DispatcherModule)
