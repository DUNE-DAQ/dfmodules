/**
 * @file DFOModule.cpp DFOModule class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DFOModule.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "dfmodules/opmon/DFOModule.pb.h"

#include "appmodel/DFOModule.hpp"
#include "confmodel/Connection.hpp"
#include "dfmessages/DataflowStatusRequest.hpp"
#include "iomanager/IOManager.hpp"
#include "logging/Logging.hpp"

#include <chrono>
#include <cstdlib>
#include <future>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "DFOModule" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10,
  TLVL_TRIGDEC_RECEIVED = 21,
  TLVL_NOTIFY_TRIGGER = 22,
  TLVL_DISPATCH_TO_TRB = 23,
  TLVL_TDTOKEN_RECEIVED = 24
};

namespace dunedaq::dfmodules {

DFOModule::DFOModule(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_queue_timeout(100)
  , m_run_number(0)
{
  register_command("conf", &DFOModule::do_conf);
  register_command("start", &DFOModule::do_start);
  register_command("drain_dataflow", &DFOModule::do_stop);
  register_command("scrap", &DFOModule::do_scrap);
}

void
DFOModule::init(std::shared_ptr<appfwk::ConfigurationManager> mcfg)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  auto mdal = mcfg->get_dal<appmodel::DFOModule>(get_name());
  if (!mdal) {
    throw appfwk::CommandFailed(ERS_HERE, "init", get_name(), "Unable to retrieve configuration object");
  }
  auto iom = iomanager::IOManager::get();

  for (auto con : mdal->get_inputs()) {
    if (con->get_data_type() == datatype_to_string<dfmessages::DataflowStatus>()) {
      m_status_connection = con->UID();
    }
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerDecision>()) {
      m_td_connection = con->UID();
    }
  }
  for (auto con : mdal->get_outputs()) {
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerInhibit>()) {
      m_busy_sender = iom->get_sender<dfmessages::TriggerInhibit>(con->UID());
    }
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerDecision>()) {
      m_trb_conn_ids.push_back(con->UID());
    }
  }

  if (m_status_connection == "") {
    throw appfwk::MissingConnection(ERS_HERE, get_name(), datatype_to_string<dfmessages::DataflowStatus>(), "input");
  }
  if (m_td_connection == "") {
    throw appfwk::MissingConnection(ERS_HERE, get_name(), datatype_to_string<dfmessages::TriggerDecision>(), "input");
  }
  if (m_busy_sender == nullptr) {
    throw appfwk::MissingConnection(ERS_HERE, get_name(), datatype_to_string<dfmessages::TriggerInhibit>(), "output");
  }

  m_dfo_conf = mdal->get_configuration();
  // these are just tests to check if the connections are ok
  iom->get_receiver<dfmessages::DataflowStatus>(m_status_connection);
  iom->get_receiver<dfmessages::TriggerDecision>(m_td_connection);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DFOModule::do_conf(const CommandData_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  m_queue_timeout = std::chrono::milliseconds(m_dfo_conf->get_general_queue_timeout_ms());
  m_stop_timeout = std::chrono::milliseconds(m_dfo_conf->get_stop_timeout_ms());
  m_request_reply_wait = std::chrono::milliseconds(m_dfo_conf->get_request_reply_wait_ms());
  m_status_watchdog_interval = std::chrono::milliseconds(m_dfo_conf->get_status_watchdog_interval_ms());
  m_dataflow_status_timeout = std::chrono::milliseconds(m_dfo_conf->get_dataflow_status_timeout_ms());

  m_reallocate_building_triggers_on_timeout = m_dfo_conf->get_reallocate_building_triggers_on_timeout();
  m_reallocate_writing_triggers_on_timeout = m_dfo_conf->get_reallocate_writing_triggers_on_timeout();

  m_td_send_retries = m_dfo_conf->get_td_send_retries();

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method, there are " << m_trb_conn_ids.size()
                                      << " TRB apps defined";
}

void
DFOModule::do_start(const CommandData_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  m_run_number = payload.value<dunedaq::daqdataformats::run_number_t>("run", 0);

  m_running_status.store(true);
  m_last_notified_busy.store(false);

  m_last_td_received = std::chrono::steady_clock::now();

  // 19-Dec-2024, KAB: check that TriggerDecision senders are ready to send. This is done
  // so that the IOManager infrastructure fetches the necessary connection details from
  // the ConnectivityService at 'start' time, instead of the first time that the sender
  // is used to send a message.  This avoids delays in the sending of the first TD in
  // the first data-taking run in a DAQ session. Such delays can lead to undesirable
  // system behavior like trigger inhibits.
  auto iom = iomanager::IOManager::get();
  if (m_busy_sender != nullptr) {
    bool is_ready = m_busy_sender->is_ready_for_sending(std::chrono::milliseconds(100));
    TLOG_DEBUG(0) << "The sender for TriggerInhibit messages " << (is_ready ? "is" : "is not") << " ready.";
  }
  for (auto trb_conn : m_trb_conn_ids) {
    auto sender = iom->get_sender<dfmessages::TriggerDecision>(trb_conn);
    if (sender != nullptr) {
      bool is_ready = sender->is_ready_for_sending(std::chrono::milliseconds(100));
      TLOG_DEBUG(0) << "The TriggerDecision sender for " << trb_conn << " " << (is_ready ? "is" : "is not")
                    << " ready.";
    }
  }
  iom->add_callback<dfmessages::DataflowStatus>(
    m_status_connection, std::bind(&DFOModule::receive_dataflow_status, this, std::placeholders::_1));

  iom->add_callback<dfmessages::TriggerDecision>(
    m_td_connection, std::bind(&DFOModule::receive_trigger_decision, this, std::placeholders::_1));

  m_status_watchdog_thread = std::make_shared<std::jthread>(std::bind_front(&DFOModule::status_watchdog_proc, this));

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DFOModule::do_stop(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  m_running_status.store(false);

  auto iom = iomanager::IOManager::get();
  iom->remove_callback<dfmessages::TriggerDecision>(m_td_connection);

  const int wait_steps = 20;
  auto step_timeout = m_stop_timeout / wait_steps;
  int step_counter = 0;
  while (m_assigned_trigger_decisions.size() > 0 && step_counter < wait_steps) {
    TLOG() << get_name() << ": stop delayed while waiting for " << m_assigned_trigger_decisions.size()
           << " TDs to completed";
    std::this_thread::sleep_for(step_timeout);
    ++step_counter;
  }

  m_status_watchdog_thread->request_stop();
  m_status_watchdog_thread->join();
  iom->remove_callback<dfmessages::DataflowStatus>(m_status_connection);

  std::list<std::shared_ptr<AssignedTriggerDecision>> remnants;
  for (auto& td : m_assigned_trigger_decisions) {
    remnants.push_back(td.second);
  }

  for (auto& r : remnants) {
    ers::error(IncompleteTriggerDecision(ERS_HERE, r->decision.trigger_number, m_run_number));
  }

  std::lock_guard<std::mutex> guard(m_trigger_counters_mutex);
  m_trigger_counters.clear();

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DFOModule::do_scrap(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  m_dataflow_statuses.clear();

  TLOG() << get_name() << " successfully scrapped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
DFOModule::receive_trigger_decision(const dfmessages::TriggerDecision& decision)
{
  TLOG_DEBUG(TLVL_TRIGDEC_RECEIVED) << get_name() << " Received TriggerDecision for trigger_number "
                                    << decision.trigger_number << " and run " << decision.run_number
                                    << " (current run is " << m_run_number << ")";
  if (decision.run_number != m_run_number) {
    ers::error(DFOModuleRunNumberMismatch(ERS_HERE, decision.run_number, m_run_number, "MLT", decision.trigger_number));
    return;
  }

  auto decision_received = std::chrono::steady_clock::now();
  ++m_received_decisions;
  m_processing_td.store(true);
  auto trigger_types = DFOTriggerCounter::unpack_types(decision.trigger_type);
  for (const auto t : trigger_types) {
    ++get_trigger_counter(t).received;
  }

  std::chrono::steady_clock::time_point decision_assigned;
  do {

    send_status_requests(decision.trigger_number);

    auto assignment = find_slot(decision);

    if (assignment == nullptr) { // this can happen if all application are in error state
      ers::error(UnableToAssign(ERS_HERE, decision.trigger_number));
      usleep(5000);
      notify_trigger_if_needed();
      continue;
    }

    TLOG_DEBUG(TLVL_TRIGDEC_RECEIVED) << get_name() << " Slot found for trigger_number " << decision.trigger_number
                                      << " on connection " << assignment->connection_name;
    decision_assigned = std::chrono::steady_clock::now();
    auto dispatch_successful = dispatch(assignment);

    if (dispatch_successful) {
      assign_trigger_decision(assignment);
      TLOG_DEBUG(TLVL_TRIGDEC_RECEIVED) << get_name() << " Assigned trigger_number " << decision.trigger_number
                                        << " to connection " << assignment->connection_name;
      break;
    } else {
      ers::error(TRBModuleAppUpdate(ERS_HERE, assignment->connection_name, "Could not send Trigger Decision"));
      // Mark this DF app status as stale so it won't be selected again
      auto it = m_dataflow_statuses.find(assignment->connection_name);
      if (it != m_dataflow_statuses.end()) {
        it->second->status_updated.store(false);
      }
    }

  } while (m_running_status.load());

  m_processing_td.store(false);
  notify_trigger_if_needed();

  m_waiting_for_decision +=
    std::chrono::duration_cast<std::chrono::microseconds>(decision_received - m_last_td_received).count();
  m_last_td_received = std::chrono::steady_clock::now();
  m_deciding_destination +=
    std::chrono::duration_cast<std::chrono::microseconds>(decision_assigned - decision_received).count();
  m_forwarding_decision +=
    std::chrono::duration_cast<std::chrono::microseconds>(m_last_td_received - decision_assigned).count();
}

void
DFOModule::generate_opmon_data()
{

  opmon::DFOInfo info;
  {
    std::lock_guard<std::mutex> lk(m_status_mutex);
    info.set_pending_trigger_decisions(m_assigned_trigger_decisions.size());
  }
  info.set_statuses_received(m_received_statuses.exchange(0));
  info.set_decisions_sent(m_sent_decisions.exchange(0));
  info.set_decisions_received(m_received_decisions.exchange(0));
  info.set_decisions_completed(m_completed_decisions.exchange(0));
  info.set_waiting_for_decision(m_waiting_for_decision.exchange(0));
  info.set_deciding_destination(m_deciding_destination.exchange(0));
  info.set_forwarding_decision(m_forwarding_decision.exchange(0));
  publish(std::move(info));

  std::lock_guard<std::mutex> guard(m_trigger_counters_mutex);
  for (auto& [type, counts] : m_trigger_counters) {
    opmon::TriggerInfo ti;
    ti.set_received(counts.received.exchange(0));
    ti.set_completed(counts.completed.exchange(0));
    auto name = dunedaq::trgdataformats::get_trigger_candidate_type_names()[type];
    publish(std::move(ti), { { "type", name } });
  }
}

bool
DFOModule::is_busy() const
{
  if (m_processing_td.load())
    return true; // DFO is busy processing a TriggerDecision

  for (auto& dfapp : m_dataflow_statuses) {
    if (!dfapp.second->status_updated.load())
      continue; // Skip stale statuses

    // Check if this DF app is not busy (has available slots)
    size_t occupied = dfapp.second->status.triggers_building.size() + dfapp.second->status.triggers_writing.size();
    if (!dfapp.second->status.is_busy && occupied < dfapp.second->status.busy_threshold) {
      return false; // At least one DF app is available
    }
  }
  return true; // All DF apps are busy
}

void
DFOModule::notify_trigger_if_needed() const
{
  // 19-Dec-2024, KAB, ELF, MaR: combined the is_busy() and notify_trigger() calls in
  // a single method (notify_trigger_if_needed), and protected the contents of the new
  // method with a mutex, to avoid a race condition in which a given is_busy() result
  // is determined, but by the time that the value is sent to the MLT, the busy state
  // has changed.
  std::lock_guard<std::mutex> guard(m_notify_trigger_mutex);

  bool busy = is_busy();
  if (busy == m_last_notified_busy.load())
    return;

  bool wasSentSuccessfully = false;

  do {
    try {
      dfmessages::TriggerInhibit message{ busy, m_run_number };
      m_busy_sender->send(std::move(message), m_queue_timeout);
      wasSentSuccessfully = true;
      TLOG_DEBUG(TLVL_NOTIFY_TRIGGER) << get_name() << " Sent BUSY status " << busy << " to trigger in run "
                                      << m_run_number;
    } catch (const ers::Issue& excpt) {
      std::ostringstream oss_warn;
      oss_warn << "Send with sender \"" << m_busy_sender->get_name() << "\" failed";
      ers::warning(iomanager::OperationFailed(ERS_HERE, oss_warn.str(), excpt));
    }

  } while (!wasSentSuccessfully && m_running_status.load());

  m_last_notified_busy.store(busy);
}

bool
DFOModule::dispatch(const std::shared_ptr<AssignedTriggerDecision>& assignment)
{

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering dispatch() method. assignment->connection_name: "
                                      << assignment->connection_name;

  bool wasSentSuccessfully = false;
  int retries = m_td_send_retries;
  auto iom = iomanager::IOManager::get();
  do {

    try {
      auto decision_copy = dfmessages::TriggerDecision(assignment->decision);
      iom->get_sender<dfmessages::TriggerDecision>(assignment->connection_name)
        ->send(std::move(decision_copy), m_queue_timeout);
      wasSentSuccessfully = true;
      ++m_sent_decisions;
      TLOG_DEBUG(TLVL_DISPATCH_TO_TRB) << get_name() << " Sent TriggerDecision for trigger_number "
                                       << decision_copy.trigger_number << " to TRB at connection "
                                       << assignment->connection_name << " for run number " << decision_copy.run_number;
    } catch (const ers::Issue& excpt) {
      std::ostringstream oss_warn;
      oss_warn << "Send to connection \"" << assignment->connection_name << "\" failed";
      ers::warning(iomanager::OperationFailed(ERS_HERE, oss_warn.str(), excpt));
    }

    retries--;

  } while (!wasSentSuccessfully && m_running_status.load() && retries > 0);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting dispatch() method";
  return wasSentSuccessfully;
}

DFOTriggerCounter&
DFOModule::get_trigger_counter(trgdataformats::TriggerCandidateData::Type type)
{
  auto it = m_trigger_counters.find(type);
  if (it != m_trigger_counters.end())
    return it->second;

  std::lock_guard<std::mutex> guard(m_trigger_counters_mutex);
  return m_trigger_counters[type];
}

void
DFOModule::receive_dataflow_status(const dfmessages::DataflowStatus& status)
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << " Received DataflowStatus from " << status.decision_destination
                              << " for trigger_number " << status.trigger_number << " in run " << status.run_number;

  ++m_received_statuses;
  {
    std::lock_guard<std::mutex> guard(m_status_mutex);
    // Update or create entry for this dataflow app
    auto it = m_dataflow_statuses.find(status.decision_destination);
    if (it != m_dataflow_statuses.end()) {
      it->second->update(status);
    } else {
      m_dataflow_statuses[status.decision_destination] =
        std::make_shared<ReceivedDataflowStatus>(status, m_dataflow_status_timeout);
    }

    if (status.trigger_number != 0) {
      m_statuses_for_trigger[status.trigger_number][status.decision_destination] = status;
    }

    for (auto& trigger : status.recently_completed_triggers) {
      if (m_assigned_trigger_decisions.count(trigger)) {
        ++m_completed_decisions;
      }
      m_statuses_for_trigger.erase(trigger);
      m_assigned_trigger_decisions.erase(trigger);
    }
  }
  notify_trigger_if_needed();
  m_status_cv.notify_all();
}

std::shared_ptr<AssignedTriggerDecision>
DFOModule::find_slot(const dfmessages::TriggerDecision& decision)
{
  // Stable algorithm to select a Dataflow application for a Trigger Decision
  // This algorithm ensures that multiple DFOs would pick the same DF app given identical state

  // Collect all candidates from known dataflow apps
  std::vector<dfmessages::DataflowStatus> candidates;

  {
    std::lock_guard<std::mutex> guard(m_status_mutex);
    for (const auto& [name, received_status] : m_statuses_for_trigger[decision.trigger_number]) {
      candidates.emplace_back(received_status);
    }
  }

  if (candidates.empty()) {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "No dataflow applications with valid status available";
    return nullptr;
  }

  // Step 1: Filter by trigger type bit mask
  std::vector<dfmessages::DataflowStatus> filtered_candidates;
  for (const auto& status : candidates) {
    // Check if this DF app supports this trigger type
    if ((status.trigger_type_mask & decision.trigger_type) != 0) {
      filtered_candidates.push_back(status);
    }
  }

  if (filtered_candidates.empty()) {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "No dataflow applications support trigger type " << decision.trigger_type;
    return nullptr;
  }

  candidates = std::move(filtered_candidates);

  // Step 2: Find DF apps with most open slots (busy_threshold - building - writing)
  if (candidates.size() > 1) {
    std::map<size_t, std::vector<dfmessages::DataflowStatus>> open_slots_map;
    for (const auto& status : candidates) {
      size_t occupied = status.triggers_building.size() + status.triggers_writing.size();
      size_t open_slots = (status.busy_threshold > occupied) ? (status.busy_threshold - occupied) : 0;
      open_slots_map[open_slots].push_back(status);
    }

    candidates = std::move(open_slots_map.rbegin()->second);
  }

  // Step 3: Find DF apps with fewest TriggerRecords processed
  if (candidates.size() > 1) {
    std::map<size_t, std::vector<dfmessages::DataflowStatus>> fewest_trs_map;
    for (const auto& status : candidates) {
      fewest_trs_map[status.trigger_records_processed].push_back(status);
    }

    candidates = std::move(fewest_trs_map.begin()->second);
  }

  // Step 4: Find DF apps with fewest bytes written
  if (candidates.size() > 1) {
    std::map<size_t, std::vector<dfmessages::DataflowStatus>> fewest_bytes_map;
    for (const auto& status : candidates) {
      fewest_bytes_map[status.data_size_written].push_back(status);
    }

    candidates = std::move(fewest_bytes_map.begin()->second);
  }

  // Step 5: Sort by name and pick first (stable tie-breaker)
  if (candidates.size() > 1) {
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
      return a.decision_destination < b.decision_destination;
    });
  }

  // Create assignment for the selected DF app
  const auto& selected_name = candidates[0].decision_destination;
  auto assignment = std::make_shared<AssignedTriggerDecision>(decision, selected_name);

  TLOG_DEBUG(TLVL_WORK_STEPS) << "Selected DF app " << selected_name << " for trigger_number "
                              << decision.trigger_number;

  return assignment;
}

void
DFOModule::assign_trigger_decision(const std::shared_ptr<AssignedTriggerDecision>& assignment)
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Assigning trigger_number " << assignment->decision.trigger_number << " to "
                              << assignment->connection_name;

  m_assigned_trigger_decisions[assignment->decision.trigger_number] = assignment;
}

void
DFOModule::status_watchdog_proc(std::stop_token stoken)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering status_watchdog_proc() method";
  while (!stoken.stop_requested()) {
    std::this_thread::sleep_for(m_status_watchdog_interval);
    std::vector<dfmessages::TriggerDecision> triggers_to_reallocate;
    {
      std::lock_guard<std::mutex> guard(m_status_mutex);
      for (auto& [name, rstatus] : m_dataflow_statuses) {
        if (!rstatus->status_updated) {
          ers::error(StaleDataflowStatus(ERS_HERE, name, m_dataflow_status_timeout.count()));
          if (m_reallocate_building_triggers_on_timeout) {
            for (auto& trigger : rstatus->status.triggers_building) {
              ers::error(ReallocatingTrigger(ERS_HERE, trigger, name));
              triggers_to_reallocate.push_back(m_assigned_trigger_decisions[trigger]->decision);
              m_statuses_for_trigger.erase(trigger);
              m_assigned_trigger_decisions.erase(trigger);
            }
          }
          if (m_reallocate_writing_triggers_on_timeout) {
            for (auto& trigger : rstatus->status.triggers_writing) {
              ers::error(ReallocatingTrigger(ERS_HERE, trigger, name));
              triggers_to_reallocate.push_back(m_assigned_trigger_decisions[trigger]->decision);
              m_statuses_for_trigger.erase(trigger);
              m_assigned_trigger_decisions.erase(trigger);
            }
          }
        }
      }
    }
    for (auto& trigger : triggers_to_reallocate) {
      receive_trigger_decision(trigger);
    }
  }
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting status_watchdog_proc() method";
}

bool
DFOModule::send_status_requests(dfmessages::trigger_number_t trigger)
{
  std::set<std::string> destinations_to_request;
  {
    std::lock_guard<std::mutex> guard(m_status_mutex);
    for (auto& [name, rstatus] : m_dataflow_statuses) {
      if (rstatus->status_updated.load()) {
        destinations_to_request.insert(rstatus->status.request_destination);
      }
    }
  }

  dfmessages::DataflowStatusRequest request;
  request.run_number = m_run_number;
  request.trigger_number = trigger;
  request.reply_destination = m_status_connection;

  for (auto& dest : destinations_to_request) {

    try {
      auto iom = iomanager::IOManager::get();
      dfmessages::DataflowStatusRequest request_copy = request;
      iom->get_sender<dfmessages::DataflowStatusRequest>(dest)->send(std::move(request_copy), m_queue_timeout);
      TLOG_DEBUG(TLVL_WORK_STEPS) << "Sent DataflowStatusRequest for trigger_number " << trigger << " to " << dest;
    } catch (const ers::Issue& excpt) {
      std::ostringstream oss_warn;
      oss_warn << "Send of DataflowStatusRequest to connection \"" << dest << "\" failed";
      ers::warning(iomanager::OperationFailed(ERS_HERE, oss_warn.str(), excpt));
    }
  }

  std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
  size_t statuses_received = 0;
  while (statuses_received < destinations_to_request.size() &&
         std::chrono::steady_clock::now() - start_time < m_request_reply_wait) {
    std::unique_lock<std::mutex> guard(m_status_mutex);
    m_status_cv.wait_for(guard, m_request_reply_wait);
    auto it = m_statuses_for_trigger.find(trigger);
    if (it != m_statuses_for_trigger.end() && !it->second.empty()) {
      statuses_received = it->second.size();
    }
  }

  return statuses_received == destinations_to_request.size();
}

} // namespace dunedaq::dfmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DFOModule)
