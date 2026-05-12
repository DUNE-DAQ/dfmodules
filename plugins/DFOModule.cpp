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
#include "iomanager/IOManager.hpp"
#include "logging/Logging.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <future>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "DFOModule" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_PEER_ANNOUNCE = 6,
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10,
  TLVL_TD_FILTER = 11,
  TLVL_DFO_DECISION = 12,
  TLVL_WATCHDOG = 13,
  TLVL_TRIGDEC_RECEIVED = 21,
  TLVL_NOTIFY_TRIGGER = 22,
  TLVL_DISPATCH_TO_TRB = 23,
  TLVL_TDTOKEN_RECEIVED = 24
};

namespace dunedaq::dfmodules {

DFOModule::DFOModule(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
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
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerDecisionToken>()) {
      m_token_connection = con->UID();
    }
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerDecision>()) {
      m_td_connection = con->UID();
    }
    if (con->get_data_type() == datatype_to_string<DFODecision>()) {
      m_dfo_decision_input_connection = con->UID();
      TLOG_DEBUG(TLVL_DFO_DECISION) << get_name() << ": Found DFODecision input connection: " << con->UID();
    }
  }
  for (auto con : mdal->get_outputs()) {
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerInhibit>()) {
      m_busy_sender = iom->get_sender<dfmessages::TriggerInhibit>(con->UID());
    }
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerDecision>()) {
      m_trb_conn_ids.push_back(con->UID());
    }
    if (con->get_data_type() == datatype_to_string<DFODecision>()) {
      m_dfo_decision_output_connections.push_back(con->UID());
      TLOG_DEBUG(TLVL_DFO_DECISION) << get_name() << ": Found DFODecision output connection: " << con->UID();
    }
  }

  if (m_token_connection.empty()) {
    throw appfwk::MissingConnection(
      ERS_HERE, get_name(), datatype_to_string<dfmessages::TriggerDecisionToken>(), "input");
  }
  if (m_td_connection.empty()) {
    throw appfwk::MissingConnection(ERS_HERE, get_name(), datatype_to_string<dfmessages::TriggerDecision>(), "input");
  }
  if (m_busy_sender == nullptr) {
    throw appfwk::MissingConnection(ERS_HERE, get_name(), datatype_to_string<dfmessages::TriggerInhibit>(), "output");
  }

  m_dfo_conf = mdal->get_configuration();
  m_consensus_enabled = m_dfo_conf->get_consensus_enabled();
  m_expected_peers = m_consensus_enabled ? m_dfo_decision_output_connections.size() : 0;

  // these are just tests to check if the connections are ok
  iom->get_receiver<dfmessages::TriggerDecisionToken>(m_token_connection);
  iom->get_receiver<dfmessages::TriggerDecision>(m_td_connection);
  if (m_consensus_enabled && !m_dfo_decision_input_connection.empty()) {
    iom->get_receiver<DFODecision>(m_dfo_decision_input_connection);
  }

  TLOG() << get_name() << ": DFOModule initialized in " << (m_consensus_enabled ? "consensus" : "standalone")
         << " mode with " << m_expected_peers << " expected peer DFO(s)";

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DFOModule::do_conf(const CommandData_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  m_queue_timeout = std::chrono::milliseconds(m_dfo_conf->get_general_queue_timeout_ms());
  m_stop_timeout = std::chrono::milliseconds(m_dfo_conf->get_stop_timeout_ms());
  m_busy_threshold = m_dfo_conf->get_busy_threshold();
  m_free_threshold = m_dfo_conf->get_free_threshold();
  m_td_send_retries = m_dfo_conf->get_td_send_retries();

  m_dfo_decision_timeout = std::chrono::milliseconds(m_dfo_conf->get_dfo_decision_timeout_ms());
  m_peer_announce_timeout = std::chrono::milliseconds(m_dfo_conf->get_peer_announce_timeout_ms());
  m_watchdog_interval = std::chrono::milliseconds(m_dfo_conf->get_watchdog_interval_ms());

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method, there are "
                                      << m_dataflow_availability.size() << " TRB apps defined";
}

void
DFOModule::do_start(const CommandData_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  m_received_tokens = 0;
  m_sent_decisions = 0;
  m_received_decisions = 0;
  m_waiting_for_decision = 0;
  m_deciding_destination = 0;
  m_forwarding_decision = 0;
  m_waiting_for_token = 0;
  m_processing_token = 0;

  {
    std::lock_guard<std::mutex> guard(m_peers_mutex);
    m_registered_peers.clear();
  }
  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    m_remote_slot_counts.clear();
  }
  {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds.clear();
  }
  m_own_index.store(0);
  m_num_dfos.store(1);

  m_run_number = payload.value<dunedaq::daqdataformats::run_number_t>("run", 0);

  m_running_status.store(true);
  m_last_notified_busy.store(false);
  m_last_assignment_it = m_dataflow_availability.end();
  m_last_token_received = m_last_td_received = std::chrono::steady_clock::now();

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
  for (const auto& trb_conn : m_trb_conn_ids) {
    auto sender = iom->get_sender<dfmessages::TriggerDecision>(trb_conn);
    if (sender != nullptr) {
      bool is_ready = sender->is_ready_for_sending(std::chrono::milliseconds(100));
      TLOG_DEBUG(0) << "The TriggerDecision sender for " << trb_conn << " " << (is_ready ? "is" : "is not")
                    << " ready.";
    }
  }

  iom->add_callback<dfmessages::TriggerDecisionToken>(
    m_token_connection, std::bind(&DFOModule::on_token, this, std::placeholders::_1));

  iom->add_callback<dfmessages::TriggerDecision>(
    m_td_connection, std::bind(&DFOModule::on_trigger_decision, this, std::placeholders::_1));

  if (m_consensus_enabled && !m_dfo_decision_input_connection.empty()) {
    iom->add_callback<DFODecision>(
      m_dfo_decision_input_connection,
      std::bind(&DFOModule::on_dfo_decision, this, std::placeholders::_1));
  }

  if (m_consensus_enabled) {
    send_peer_announcement();

    if (m_expected_peers > 0) {
      std::unique_lock<std::mutex> lock(m_peers_mutex);
      bool all_peers_ready = m_peers_cv.wait_for(lock, m_peer_announce_timeout, [this] {
        return m_registered_peers.size() >= m_expected_peers;
      });
      if (!all_peers_ready) {
        ers::warning(DFOConsensusPeerTimeout(
          ERS_HERE, get_name(), m_expected_peers, m_registered_peers.size()));
      }
    }

    compute_partition();
    ers::info(DFOConsensusPartitionInfo(ERS_HERE, get_name(), m_own_index.load(), m_num_dfos.load()));

    m_watchdog_running.store(true);
    m_watchdog_thread = std::thread(&DFOModule::watchdog_thread_func, this);
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DFOModule::do_stop(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  m_watchdog_running.store(false);
  if (m_watchdog_thread.joinable()) {
    m_watchdog_thread.join();
  }

  m_running_status.store(false);

  auto iom = iomanager::IOManager::get();
  iom->remove_callback<dfmessages::TriggerDecision>(m_td_connection);

  const int wait_steps = 20;
  auto step_timeout = m_stop_timeout / wait_steps;
  int step_counter = 0;
  while (!is_empty() && step_counter < wait_steps) {
    TLOG() << get_name() << ": stop delayed while waiting for " << used_slots() << " TDs to complete";
    std::this_thread::sleep_for(step_timeout);
    ++step_counter;
  }

  iom->remove_callback<dfmessages::TriggerDecisionToken>(m_token_connection);
  if (m_consensus_enabled && !m_dfo_decision_input_connection.empty()) {
    iom->remove_callback<DFODecision>(m_dfo_decision_input_connection);
  }

  std::list<std::shared_ptr<AssignedTriggerDecision>> remnants;
  for (auto& app : m_dataflow_availability) {
    auto temp = app.second->flush();
    for (auto& td : temp) {
      remnants.push_back(td);
    }
  }

  for (auto& r : remnants) {
    ers::error(IncompleteTriggerDecision(ERS_HERE, r->decision.trigger_number, m_run_number));
  }

  {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds.clear();
  }
  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    m_remote_slot_counts.clear();
  }
  m_own_index.store(0);
  m_num_dfos.store(1);

  std::lock_guard<std::mutex> guard(m_trigger_counters_mutex);
  m_trigger_counters.clear();

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DFOModule::do_scrap(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  m_dataflow_availability.clear();
  {
    std::lock_guard<std::mutex> guard(m_peers_mutex);
    m_registered_peers.clear();
  }
  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    m_remote_slot_counts.clear();
  }
  {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds.clear();
  }

  TLOG() << get_name() << " successfully scrapped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
DFOModule::on_token(const dfmessages::TriggerDecisionToken& token)
{
  receive_trigger_complete_token(token);
}

void
DFOModule::on_trigger_decision(const dfmessages::TriggerDecision& decision)
{
  if (!m_consensus_enabled) {
    receive_trigger_decision(decision);
    return;
  }

  {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds[decision.trigger_number] = { decision, std::chrono::steady_clock::now() };
  }

  size_t num_dfos = m_num_dfos.load();
  size_t own_index = m_own_index.load();

  if (num_dfos > 1 && (decision.trigger_number % num_dfos) != own_index) {
    TLOG_DEBUG(TLVL_TD_FILTER) << get_name() << ": Buffered trigger_number " << decision.trigger_number
                               << " awaiting DFODecision from partition "
                               << (decision.trigger_number % num_dfos);
    return;
  }

  receive_trigger_decision(decision);
}

void
DFOModule::on_dfo_decision(const DFODecision& msg)
{
  if (!m_consensus_enabled) {
    return;
  }

  TLOG_DEBUG(TLVL_DFO_DECISION) << get_name() << ": Received DFODecision from " << msg.source_dfo_name
                                << " trigger=" << msg.trigger_number << " trb=" << msg.trb_connection_name
                                << " slots=" << msg.trb_slot_count
                                << " completion=" << std::boolalpha << msg.is_completion;

  if (msg.run_number == 0 && msg.trigger_number == s_peer_announce_magic) {
    TLOG_DEBUG(TLVL_PEER_ANNOUNCE) << get_name() << ": Received peer announcement from " << msg.source_dfo_name;
    bool newly_registered = false;
    {
      std::lock_guard<std::mutex> guard(m_peers_mutex);
      auto [it, inserted] = m_registered_peers.insert(msg.source_dfo_name);
      newly_registered = inserted;
    }
    m_peers_cv.notify_all();

    if (newly_registered) {
      compute_partition();
      ers::info(DFOConsensusPartitionInfo(ERS_HERE, get_name(), m_own_index.load(), m_num_dfos.load()));
    }
    return;
  }

  if (msg.run_number != m_run_number) {
    return;
  }

  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    m_remote_slot_counts[msg.source_dfo_name][msg.trb_connection_name] = msg.trb_slot_count;
  }

  if (!msg.is_completion) {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds.erase(msg.trigger_number);
  }

  notify_trigger_if_needed();
}

void
DFOModule::send_peer_announcement()
{
  if (!m_consensus_enabled || m_dfo_decision_output_connections.empty()) {
    return;
  }

  DFODecision announcement;
  announcement.run_number = 0;
  announcement.trigger_number = s_peer_announce_magic;
  announcement.trb_connection_name = "";
  announcement.trb_slot_count = 0;
  announcement.source_dfo_name = get_name();
  announcement.is_completion = true;

  auto iom = iomanager::IOManager::get();
  for (const auto& conn : m_dfo_decision_output_connections) {
    try {
      auto announcement_copy = announcement;
      iom->get_sender<DFODecision>(conn)->send(std::move(announcement_copy), m_queue_timeout);
      TLOG_DEBUG(TLVL_PEER_ANNOUNCE) << get_name() << ": Sent peer announcement to " << conn;
    } catch (const ers::Issue& excpt) {
      ers::warning(excpt);
    }
  }
}

void
DFOModule::compute_partition()
{
  std::vector<std::string> ensemble;
  {
    std::lock_guard<std::mutex> guard(m_peers_mutex);
    ensemble.push_back(get_name());
    for (const auto& peer : m_registered_peers) {
      ensemble.push_back(peer);
    }
  }

  std::sort(ensemble.begin(), ensemble.end());

  auto it = std::find(ensemble.begin(), ensemble.end(), get_name());
  size_t own_index = (it != ensemble.end()) ? static_cast<size_t>(std::distance(ensemble.begin(), it)) : 0;

  m_own_index.store(own_index);
  m_num_dfos.store(ensemble.size());

  TLOG_DEBUG(TLVL_TD_FILTER) << get_name() << ": Partition computed: index=" << own_index << " of "
                             << ensemble.size() << " DFO(s)";
}

void
DFOModule::receive_trigger_decision(const dfmessages::TriggerDecision& decision)
{
  TLOG_DEBUG(TLVL_TRIGDEC_RECEIVED) << get_name() << " Received TriggerDecision for trigger_number "
                                    << decision.trigger_number << " and run " << decision.run_number
                                    << " (current run is " << m_run_number << ")";
  if (decision.run_number != m_run_number) {
    ers::error(DFOModuleRunNumberMismatch(
      ERS_HERE, decision.run_number, m_run_number, "MLT", decision.trigger_number));
    return;
  }

  auto decision_received = std::chrono::steady_clock::now();
  ++m_received_decisions;
  auto trigger_types = unpack_types(decision.trigger_type);
  for (const auto t : trigger_types) {
    ++get_trigger_counter(t).received;
  }

  std::chrono::steady_clock::time_point decision_assigned;
  do {

    auto assignment = find_slot(decision);

    if (assignment == nullptr) { // this can happen if all application are in error state
      ers::error(UnableToAssign(ERS_HERE, decision.trigger_number));
      usleep(500);
      notify_trigger_if_needed();
      continue;
    }

    TLOG_DEBUG(TLVL_TRIGDEC_RECEIVED) << get_name() << " Slot found for trigger_number " << decision.trigger_number
                                      << " on connection " << assignment->connection_name
                                      << ", number of used slots is " << used_slots();
    decision_assigned = std::chrono::steady_clock::now();
    auto dispatch_successful = dispatch(assignment);

    if (dispatch_successful) {
      assign_trigger_decision(assignment);
      TLOG_DEBUG(TLVL_TRIGDEC_RECEIVED) << get_name() << " Assigned trigger_number " << decision.trigger_number
                                        << " to connection " << assignment->connection_name;
      break;
    } else {
      ers::error(
        TRBModuleAppUpdate(ERS_HERE, assignment->connection_name, "Could not send Trigger Decision"));
      m_dataflow_availability[assignment->connection_name]->set_in_error(true);
    }

  } while (m_running_status.load());

  notify_trigger_if_needed();

  m_waiting_for_decision +=
    std::chrono::duration_cast<std::chrono::microseconds>(decision_received - m_last_td_received).count();
  m_last_td_received = std::chrono::steady_clock::now();
  m_deciding_destination +=
    std::chrono::duration_cast<std::chrono::microseconds>(decision_assigned - decision_received).count();
  m_forwarding_decision +=
    std::chrono::duration_cast<std::chrono::microseconds>(m_last_td_received - decision_assigned).count();
}

std::shared_ptr<AssignedTriggerDecision>
DFOModule::find_slot(const dfmessages::TriggerDecision& decision)
{

  // this find_slot assings the decision with a round-robin logic
  // across all the available applications.
  // Applications in error are skipped.
  // we only probe the applications once.
  // if they are all unavailable the assignment is set to
  // the application with the lowest used slots
  // returning a nullptr will be considered as an error
  // from the upper level code

  std::shared_ptr<AssignedTriggerDecision> output = nullptr;
  auto minimum_occupied = m_dataflow_availability.end();
  size_t minimum = std::numeric_limits<size_t>::max();
  unsigned int counter = 0;

  auto candidate_it = m_last_assignment_it;
  if (candidate_it == m_dataflow_availability.end())
    candidate_it = m_dataflow_availability.begin();

  while (output == nullptr && counter < m_dataflow_availability.size()) {

    ++counter;
    ++candidate_it;
    if (candidate_it == m_dataflow_availability.end())
      candidate_it = m_dataflow_availability.begin();

    // get rid of the applications in error state
    if (candidate_it->second->is_in_error()) {
      continue;
    }

    // monitor
    auto slots = candidate_it->second->used_slots();
    if (slots < minimum) {
      minimum = slots;
      minimum_occupied = candidate_it;
    }

    if (candidate_it->second->is_busy())
      continue;

    output = candidate_it->second->make_assignment(decision);
    m_last_assignment_it = candidate_it;
  }

  if (!output) {
    // in this case all applications were busy
    // so we assign the decision to that with the lowest
    // number of assignments
    if (minimum_occupied != m_dataflow_availability.end()) {
      output = minimum_occupied->second->make_assignment(decision);
      m_last_assignment_it = minimum_occupied;
      ers::warning(AssignedToBusyApp(ERS_HERE, decision.trigger_number, minimum_occupied->first, minimum));
    }
  }

  if (output != nullptr) {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "Assigned TriggerDecision with trigger number " << decision.trigger_number
                                << " to TRB at connection " << output->connection_name;
  }
  return output;
}

void
DFOModule::generate_opmon_data()
{

  opmon::DFOInfo info;
  info.set_tokens_received(m_received_tokens.exchange(0));
  info.set_decisions_sent(m_sent_decisions.exchange(0));
  info.set_decisions_received(m_received_decisions.exchange(0));
  info.set_waiting_for_decision(m_waiting_for_decision.exchange(0));
  info.set_deciding_destination(m_deciding_destination.exchange(0));
  info.set_forwarding_decision(m_forwarding_decision.exchange(0));
  info.set_waiting_for_token(m_waiting_for_token.exchange(0));
  info.set_processing_token(m_processing_token.exchange(0));
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

void
DFOModule::receive_trigger_complete_token(const dfmessages::TriggerDecisionToken& token)
{
  if (token.run_number == 0 && token.trigger_number == 0) {
    if (m_dataflow_availability.count(token.decision_destination) == 0) {
      TLOG_DEBUG(TLVL_CONFIG) << "Creating dataflow availability struct for uid " << token.decision_destination;
      auto entry = m_dataflow_availability[token.decision_destination] =
        std::make_shared<TriggerRecordBuilderData>(token.decision_destination, m_busy_threshold, m_free_threshold);
      register_node(token.decision_destination, entry);
    } else {
      TLOG() << TRBModuleAppUpdate(ERS_HERE, token.decision_destination, "Has reconnected");
      auto app_it = m_dataflow_availability.find(token.decision_destination);
      app_it->second->set_in_error(false);
    }
    return;
  }

  TLOG_DEBUG(TLVL_TDTOKEN_RECEIVED) << get_name() << " Received TriggerDecisionToken for trigger_number "
                                    << token.trigger_number << " and run " << token.run_number
                                    << " (current run is " << m_run_number << ")";
  // add a check to see if the application data found
  if (token.run_number != m_run_number) {
    std::ostringstream oss_source;
    oss_source << "TRB at connection " << token.decision_destination;
    ers::error(DFOModuleRunNumberMismatch(
      ERS_HERE, token.run_number, m_run_number, oss_source.str(), token.trigger_number));
    return;
  }

  auto app_it = m_dataflow_availability.find(token.decision_destination);
  // check if application data exists;
  if (app_it == m_dataflow_availability.end()) {
    ers::error(UnknownTokenSource(ERS_HERE, token.decision_destination));
    return;
  }

  ++m_received_tokens;
  auto callback_start = std::chrono::steady_clock::now();

  try {
    auto dec_ptr = app_it->second->complete_assignment(token.trigger_number, m_metadata_function);
    auto trigger_types = unpack_types(dec_ptr->decision.trigger_type);
    for (const auto t : trigger_types)
      ++get_trigger_counter(t).completed;

    if (m_consensus_enabled) {
      broadcast_dfo_decision(token.trigger_number, token.decision_destination, app_it->second->used_slots(), true);
    }
  } catch (AssignedTriggerDecisionNotFound const& err) {
    ers::error(err);
  }

  if (app_it->second->is_in_error()) {
    TLOG() << TRBModuleAppUpdate(ERS_HERE, token.decision_destination, "Has reconnected");
    app_it->second->set_in_error(false);
  }

  notify_trigger_if_needed();

  m_waiting_for_token +=
    std::chrono::duration_cast<std::chrono::microseconds>(callback_start - m_last_token_received).count();
  m_last_token_received = std::chrono::steady_clock::now();
  m_processing_token +=
    std::chrono::duration_cast<std::chrono::microseconds>(m_last_token_received - callback_start).count();
}

bool
DFOModule::is_busy() const
{
  if (!m_consensus_enabled) {
    for (auto& dfapp : m_dataflow_availability) {
      if (!dfapp.second->is_busy())
        return false;
    }
    return true;
  }

  return is_globally_busy();
}

bool
DFOModule::is_globally_busy() const
{
  if (m_dataflow_availability.empty()) {
    return true;
  }

  std::map<std::string, size_t> peer_totals;
  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    for (const auto& peer_slots : m_remote_slot_counts) {
      for (const auto& [trb_conn, count] : peer_slots.second) {
        peer_totals[trb_conn] += count;
      }
    }
  }

  for (const auto& [trb_conn, trb_data] : m_dataflow_availability) {
    size_t total_slots = trb_data->used_slots();
    auto peer_it = peer_totals.find(trb_conn);
    if (peer_it != peer_totals.end()) {
      total_slots += peer_it->second;
    }

    if (total_slots < m_busy_threshold) {
      return false;
    }
  }
  return true;
}

bool
DFOModule::is_empty() const
{
  for (auto& dfapp : m_dataflow_availability) {
    if (dfapp.second->used_slots() != 0)
      return false;
  }
  return true;
}

size_t
DFOModule::used_slots() const
{
  size_t total = 0;
  for (auto& dfapp : m_dataflow_availability) {
    total += dfapp.second->used_slots();
  }
  return total;
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
      iom->get_sender<dfmessages::TriggerDecision>(assignment->connection_name)->send(std::move(decision_copy),
                                                                                       m_queue_timeout);
      wasSentSuccessfully = true;
      ++m_sent_decisions;
      TLOG_DEBUG(TLVL_DISPATCH_TO_TRB) << get_name() << " Sent TriggerDecision for trigger_number "
                                       << assignment->decision.trigger_number << " to TRB at connection "
                                       << assignment->connection_name << " for run number "
                                       << assignment->decision.run_number;
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

void
DFOModule::broadcast_dfo_decision(daqdataformats::trigger_number_t trigger_number,
                                  const std::string& trb_conn,
                                  size_t trb_slot_count,
                                  bool is_completion)
{
  if (!m_consensus_enabled || m_dfo_decision_output_connections.empty())
    return;

  DFODecision msg;
  msg.run_number = m_run_number;
  msg.trigger_number = trigger_number;
  msg.trb_connection_name = trb_conn;
  msg.trb_slot_count = trb_slot_count;
  msg.source_dfo_name = get_name();
  msg.is_completion = is_completion;

  auto iom = iomanager::IOManager::get();
  for (const auto& conn : m_dfo_decision_output_connections) {
    try {
      auto msg_copy = msg;
      iom->get_sender<DFODecision>(conn)->send(std::move(msg_copy), m_queue_timeout);
      TLOG_DEBUG(TLVL_DFO_DECISION) << get_name() << ": Sent DFODecision to " << conn
                                    << " trigger=" << trigger_number << " trb=" << trb_conn
                                    << " slots=" << trb_slot_count
                                    << " completion=" << std::boolalpha << is_completion;
    } catch (const ers::Issue& excpt) {
      ers::warning(excpt);
    }
  }
}

void
DFOModule::assign_trigger_decision(const std::shared_ptr<AssignedTriggerDecision>& assignment)
{
  auto& trb_data = m_dataflow_availability[assignment->connection_name];
  trb_data->add_assignment(assignment);

  if (m_consensus_enabled) {
    broadcast_dfo_decision(
      assignment->decision.trigger_number, assignment->connection_name, trb_data->used_slots(), false);

    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds.erase(assignment->decision.trigger_number);
  }
}

void
DFOModule::watchdog_thread_func()
{
  while (m_watchdog_running.load()) {
    std::this_thread::sleep_for(m_watchdog_interval);

    if (!m_consensus_enabled || !m_running_status.load()) {
      continue;
    }

    std::vector<std::pair<daqdataformats::trigger_number_t, PendingTD>> timed_out;
    auto now = std::chrono::steady_clock::now();

    {
      std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
      for (const auto& entry : m_pending_tds) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.second.received_at);
        if (age > m_dfo_decision_timeout) {
          timed_out.emplace_back(entry.first, entry.second);
        }
      }
    }

    for (const auto& [trigger_number, pending] : timed_out) {
      size_t failed_index = m_num_dfos.load() > 0 ? (trigger_number % m_num_dfos.load()) : 0;
      handle_peer_failure(failed_index, trigger_number);

      std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
      auto it = m_pending_tds.find(trigger_number);
      if (it != m_pending_tds.end()) {
        TLOG_DEBUG(TLVL_WATCHDOG) << get_name() << ": Reprocessing trigger_number " << trigger_number
                                  << " after failover";
        auto decision_copy = it->second.decision;
        m_pending_tds.erase(it);
        receive_trigger_decision(decision_copy);
      }
    }
  }
}

void
DFOModule::handle_peer_failure(size_t failed_index, daqdataformats::trigger_number_t trigger_number)
{
  std::string failed_peer_name;
  {
    std::lock_guard<std::mutex> guard(m_peers_mutex);

    std::vector<std::string> ensemble;
    ensemble.push_back(get_name());
    for (const auto& peer : m_registered_peers) {
      ensemble.push_back(peer);
    }
    std::sort(ensemble.begin(), ensemble.end());

    if (failed_index < ensemble.size()) {
      failed_peer_name = ensemble[failed_index];
      if (!failed_peer_name.empty() && failed_peer_name != get_name()) {
        m_registered_peers.erase(failed_peer_name);
      }
    }
  }

  if (!failed_peer_name.empty() && failed_peer_name != get_name()) {
    ers::warning(DFOConsensusFailover(ERS_HERE, get_name(), failed_peer_name, trigger_number));
  }

  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    if (!failed_peer_name.empty()) {
      m_remote_slot_counts.erase(failed_peer_name);
    }
  }

  compute_partition();
  ers::info(DFOConsensusPartitionInfo(ERS_HERE, get_name(), m_own_index.load(), m_num_dfos.load()));
}

} // namespace dunedaq::dfmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DFOModule)
