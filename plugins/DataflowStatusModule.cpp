/**
 * @file DataflowStatusModule.cpp DataflowStatusModule class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DataflowStatusModule.hpp"
#include "appmodel/DataflowStatusModule.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "dfmodules/opmon/DataflowStatusModule.pb.h"

#include <memory>
#include <string>
#include <utility>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "DataflowStatusModule" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10,
  TLVL_MESSAGE_RECEIVED = 21,
  TLVL_HEARTBEAT = 22,
  TLVL_SEND_STATE = 23,
};

namespace dunedaq::dfmodules {

DataflowStatusModule::DataflowStatusModule(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_status_update_thread(std::bind(&DataflowStatusModule::status_update_thread, this, std::placeholders::_1))
  , m_heartbeat_thread(std::bind(&DataflowStatusModule::status_heartbeat_thread, this, std::placeholders::_1))
{
  register_command("conf", &DataflowStatusModule::do_conf);
  register_command("start", &DataflowStatusModule::do_start);
  register_command("stop", &DataflowStatusModule::do_stop);
  register_command("scrap", &DataflowStatusModule::do_scrap);
}

DataflowStatusModule::~DataflowStatusModule()
{
  if(m_status_update_thread.thread_running()) {
    m_status_update_thread.stop_working_thread();
  }
  if (m_heartbeat_thread.thread_running()) {
    m_heartbeat_thread.stop_working_thread();
  }
  m_status_update_cv.notify_all();
}

void
DataflowStatusModule::init(std::shared_ptr<appfwk::ConfigurationManager> mcfg)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  auto mdal = mcfg->get_dal<appmodel::DataflowStatusModule>(get_name());
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
    if (con->get_data_type() == datatype_to_string<dfmessages::DataflowStatusRequest>()) {
      m_status_request_connection = con->UID();
    }
    if (con->get_data_type() == datatype_to_string<dfmessages::TRBCompletion>()) {
      m_trb_completion_connection = con->UID();
    }
  }
  for (auto con : mdal->get_outputs()) {
    if (con->get_data_type() == datatype_to_string<dfmessages::DataflowStatus>()) {
      m_known_dfos.insert(con->UID());
    }
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerDecision>()) {
      m_trigger_decision_sender = iom->get_sender<dfmessages::TriggerDecision>(con->UID());
    }
  }

  if (m_token_connection == "") {
    throw appfwk::MissingConnection(
      ERS_HERE, get_name(), datatype_to_string<dfmessages::TriggerDecisionToken>(), "input");
  }
  if (m_td_connection == "") {
    throw appfwk::MissingConnection(ERS_HERE, get_name(), datatype_to_string<dfmessages::TriggerDecision>(), "input");
  }
  if (m_trb_completion_connection == "") {
    throw appfwk::MissingConnection(ERS_HERE, get_name(), datatype_to_string<dfmessages::TRBCompletion>(), "input");
  }
  if (m_status_request_connection == "") {
    throw appfwk::MissingConnection(
      ERS_HERE, get_name(), datatype_to_string<dfmessages::DataflowStatusRequest>(), "input");
  }
  if (m_trigger_decision_sender == nullptr) {
    throw appfwk::MissingConnection(ERS_HERE, get_name(), datatype_to_string<dfmessages::TriggerDecision>(), "output");
  }

  m_current_status.decision_destination = m_td_connection;
  m_current_status.request_destination = m_status_request_connection;
  m_conf = mdal->get_configuration();
  // these are just tests to check if the connections are ok
  iom->get_receiver<dfmessages::TriggerDecisionToken>(m_token_connection);
  iom->get_receiver<dfmessages::TriggerDecision>(m_td_connection);
  iom->get_receiver<dfmessages::DataflowStatusRequest>(m_status_request_connection);
  iom->get_receiver<dfmessages::TRBCompletion>(m_trb_completion_connection);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DataflowStatusModule::generate_opmon_data()
{
  opmon::DataflowStatusInfo info;
  info.set_decisions_received(m_num_trigger_decisions_received.exchange(0));
  info.set_trb_completions_received(m_num_trb_completions_received.exchange(0));
  info.set_tokens_received(m_num_trigger_decision_tokens_received.exchange(0));
  info.set_requests_received(m_num_status_requests_received.exchange(0));
  info.set_status_messages_sent(m_num_status_messages_sent.exchange(0));
  info.set_decisions_sent(m_num_trigger_decisions_sent.exchange(0));

  info.set_trigger_decisions_completed(m_complete_counter.exchange(0));

  auto free = 0;
  {
    std::unique_lock<std::mutex> lk(m_status_mutex);

    uint64_t time = 0;
    bool first = true;
        info.set_min_time_since_assignment(0);
        info.set_max_time_since_assignment(0);

    auto current_time = std::chrono::steady_clock::now();
    for (const auto& [trigger_id, trigger_status] : m_trigger_statuses) {
      int64_t time_since_assignment =
        std::chrono::duration_cast<std::chrono::microseconds>(current_time - trigger_status.decision_received).count();
      time += time_since_assignment;
      if (first || time_since_assignment < info.min_time_since_assignment())
        info.set_min_time_since_assignment(time_since_assignment);
      if (first || time_since_assignment > info.max_time_since_assignment())
        info.set_max_time_since_assignment(time_since_assignment);
      first = false;
    }
    info.set_total_time_since_assignment(time);

    info.set_is_busy(m_current_status.is_busy);
    info.set_slots_building(m_current_status.triggers_building.size());
    info.set_slots_writing(m_current_status.triggers_writing.size());
    free = m_current_status.is_busy ? 0
                                    : m_current_status.busy_threshold - m_current_status.triggers_building.size() -
                                        m_current_status.triggers_writing.size();
    info.set_slots_free(free);
  }

  info.set_pending_trigger_decision_time(m_pending_trigger_decision_time_us.exchange(0));
  info.set_pending_trb_completion_time(m_pending_trb_completion_time_us.exchange(0));
  info.set_pending_token_time(m_pending_trigger_decision_token_time_us.exchange(0));

  info.set_total_completion_time(m_completion_time_us.exchange(0));
  info.set_min_completion_time(m_min_completion_time_us.exchange(0));
  info.set_max_completion_time(m_max_completion_time_us.exchange(0));

  info.set_duplicate_decisions_received(m_num_duplicate_decisions_received.exchange(0));
  info.set_pending_trb_completion_timeouts(m_num_pending_trb_completion_timeouts.exchange(0));
  info.set_pending_trigger_decision_token_timeouts(m_num_pending_trigger_decision_token_timeouts.exchange(0));

  publish(std::move(info));
}

void
DataflowStatusModule::do_conf(const CommandData_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  m_heartbeat_interval = std::chrono::milliseconds(m_conf->get_heartbeat_interval_ms());
  m_stop_timeout = std::chrono::milliseconds(m_conf->get_stop_timeout_ms());
  m_td_queue_timeout = std::chrono::milliseconds(m_conf->get_td_queue_timeout_ms());
  m_pending_message_timeout = std::chrono::milliseconds(m_conf->get_pending_message_timeout_ms());
  m_snapshot_history_size = m_conf->get_snapshot_history_size();
  m_completed_trigger_history_size = m_conf->get_completed_trigger_history_size();

  m_current_status.busy_threshold = m_conf->get_busy_threshold();
  m_current_status.free_threshold = m_conf->get_free_threshold();
  m_current_status.trigger_type_mask = static_cast<dfmessages::trigger_type_t>(m_conf->get_trigger_type_mask());

  auto iom = iomanager::IOManager::get();
  iom->add_callback<dfmessages::DataflowStatusRequest>(
    m_status_request_connection, std::bind(&DataflowStatusModule::receive_status_request, this, std::placeholders::_1));

  m_heartbeat_thread.start_working_thread();
  m_status_update_thread.start_working_thread();
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
DataflowStatusModule::do_start(const CommandData_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  m_current_status.trigger_id.run_number = payload.value<dunedaq::daqdataformats::run_number_t>("run", 0);

  {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    m_status_snapshots.clear();
    m_trigger_statuses.clear();
    m_current_status.triggers_building.clear();
    m_current_status.triggers_writing.clear();
    m_current_status.recently_completed_triggers.clear();
    m_current_status.trigger_records_processed = 0;
    m_current_status.data_size_written = 0;
  }

  // 19-Dec-2024, KAB: check that TriggerDecision senders are ready to send. This is done
  // so that the IOManager infrastructure fetches the necessary connection details from
  // the ConnectivityService at 'start' time, instead of the first time that the sender
  // is used to send a message.  This avoids delays in the sending of the first TD in
  // the first data-taking run in a DAQ session. Such delays can lead to undesirable
  // system behavior like trigger inhibits.
  auto iom = iomanager::IOManager::get();
  if (m_trigger_decision_sender != nullptr) {
    bool is_ready = m_trigger_decision_sender->is_ready_for_sending(std::chrono::milliseconds(100));
    TLOG_DEBUG(0) << "The sender for TriggerDecision messages " << (is_ready ? "is" : "is not") << " ready.";
  }

  for (auto& known_dfo : m_known_dfos) {

    auto sender = iom->get_sender<dfmessages::DataflowStatus>(known_dfo);
    if (sender != nullptr) {
      bool is_ready = sender->is_ready_for_sending(std::chrono::milliseconds(100));
      TLOG_DEBUG(0) << "The DataflowStatus sender for " << known_dfo << " " << (is_ready ? "is" : "is not")
                    << " ready.";
    }
  }

  iom->add_callback<dfmessages::TriggerDecisionToken>(
    m_token_connection, std::bind(&DataflowStatusModule::receive_trigger_decision_token, this, std::placeholders::_1));

  iom->add_callback<dfmessages::TriggerDecision>(
    m_td_connection, std::bind(&DataflowStatusModule::receive_trigger_decision, this, std::placeholders::_1));

  iom->add_callback<dfmessages::TRBCompletion>(
    m_trb_completion_connection, std::bind(&DataflowStatusModule::receive_trb_completion, this, std::placeholders::_1));

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DataflowStatusModule::do_stop(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  auto iom = iomanager::IOManager::get();
  // Stop receiving new TDs
  iom->remove_callback<dfmessages::TriggerDecision>(m_td_connection);

  // Wait for TRB to finish building TriggerRecords
  const int wait_steps = 20;
  auto step_timeout = m_stop_timeout / wait_steps;
  int step_counter = 0;
  while (m_current_status.triggers_building.size() > 0 && step_counter < wait_steps) {
    TLOG() << get_name() << ": stop delayed while waiting for " << m_current_status.triggers_building.size()
           << " TDs to completed (building)";
    std::this_thread::sleep_for(step_timeout);
    ++step_counter;
  }
  iom->remove_callback<dfmessages::TRBCompletion>(m_trb_completion_connection);

  // Wait for DataWriter(s) to finish writing data and sending TriggerDecisionTokens
  while (m_current_status.triggers_writing.size() > 0 && step_counter < wait_steps) {
    TLOG() << get_name() << ": stop delayed while waiting for " << m_current_status.triggers_writing.size()
           << " TDs to completed (writing)";
    std::this_thread::sleep_for(step_timeout);
    ++step_counter;
  }
  iom->remove_callback<dfmessages::TriggerDecisionToken>(m_token_connection);

  m_current_status.trigger_id.run_number = 0;

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DataflowStatusModule::do_scrap(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  if(m_status_update_thread.thread_running()) {
    m_status_update_thread.stop_working_thread();
  }
  if (m_heartbeat_thread.thread_running()) {
    m_heartbeat_thread.stop_working_thread();
  }

  TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Removing request callback";
  auto iom = iomanager::IOManager::get();
  iom->remove_callback<dfmessages::DataflowStatusRequest>(m_status_request_connection);

  TLOG() << get_name() << " successfully scrapped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
DataflowStatusModule::receive_status_request(const dfmessages::DataflowStatusRequest& request)
{
  TLOG_DEBUG(TLVL_MESSAGE_RECEIVED) << get_name() << " Received DataflowStatusRequest for run "
                                    << request.trigger_id.run_number << " (current run is "
                                    << m_current_status.trigger_id.run_number << ")";
  if (request.trigger_id.run_number != m_current_status.trigger_id.run_number) {
    return;
  }
  ++m_num_status_requests_received;

  {
    std::unique_lock<std::mutex> lock(m_status_mutex);
    if (m_status_snapshots.count(request.trigger_id) == 0 ||
        request.iteration_number > m_status_snapshots[request.trigger_id].iteration_number) {
      m_status_snapshots[request.trigger_id] = m_current_status;
      m_status_snapshots[request.trigger_id].trigger_id = request.trigger_id;
      m_status_snapshots[request.trigger_id].iteration_number = request.iteration_number;

      while (m_status_snapshots.size() > m_snapshot_history_size) {
        m_status_snapshots.erase(m_status_snapshots.begin());
      }
    }
    m_known_dfos.insert(request.reply_destination);
  }

  send_dataflow_status_update(request.reply_destination, request.trigger_id.trigger_number);
}

void
DataflowStatusModule::receive_trigger_decision(const dfmessages::TriggerDecision& decision)
{
  TLOG_DEBUG(TLVL_MESSAGE_RECEIVED) << get_name() << " Received TriggerDecision for trigger_number "
                                    << decision.trigger_number << " and run " << decision.run_number
                                    << " (current run is " << m_current_status.trigger_id.run_number << ")";
  if (decision.run_number != m_current_status.trigger_id.run_number) {
    ers::error(TriggerDecisionIncorrectRun(
      ERS_HERE, get_name(), decision.trigger_number, decision.run_number, m_current_status.trigger_id.run_number));
    return;
  }

  std::lock_guard<std::mutex> lock(m_pending_trigger_decision_mutex);
  m_pending_trigger_decisions.emplace_back(decision, std::chrono::steady_clock::now());
}

void
DataflowStatusModule::receive_trb_completion(const dfmessages::TRBCompletion& completion)
{
  TLOG_DEBUG(TLVL_MESSAGE_RECEIVED) << get_name() << " Received TRBCompletion for trigger record "
                                    << completion.trigger_id.trigger_number << "." << completion.sequence_number << "/"
                                    << completion.trigger_id.run_number << " (current run is "
                                    << m_current_status.trigger_id.run_number << ")";
  std::lock_guard<std::mutex> lock(m_pending_trb_completion_mutex);
  m_pending_trb_completions.emplace_back(completion, std::chrono::steady_clock::now());
}

void
DataflowStatusModule::receive_trigger_decision_token(const dfmessages::TriggerDecisionToken& token)
{
  TLOG_DEBUG(TLVL_MESSAGE_RECEIVED) << get_name() << " Received TriggerDecisionToken for trigger record "
                                    << token.trigger_id.trigger_number << "." << token.sequence_number << "/"
                                    << token.trigger_id.run_number << " (current run is "
                                    << m_current_status.trigger_id.run_number << ")";
  std::lock_guard<std::mutex> lock(m_pending_trigger_decision_token_mutex);
  m_pending_trigger_decision_tokens.emplace_back(token, std::chrono::steady_clock::now());
}

void
DataflowStatusModule::process_trigger_decision(dfmessages::TriggerDecision& decision,
                                               std::chrono::steady_clock::time_point received)
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << " Processing TriggerDecision for trigger_number "
                              << decision.trigger_number << " and run " << decision.run_number;
  {
    std::unique_lock<std::mutex> lock(m_status_mutex);

    dfmessages::TriggerId trigger_id{ decision.run_number, decision.trigger_number };

    if (m_trigger_statuses.count(trigger_id) > 0) {
      TLOG() << DuplicateTriggerDecision(ERS_HERE, get_name(), decision.trigger_number, decision.run_number);
      ++m_num_duplicate_decisions_received;
      return;
    }

    ++m_num_trigger_decisions_received;

    auto now = std::chrono::steady_clock::now();
    m_trigger_statuses[trigger_id].decision_received = received;
    m_current_status.triggers_building.insert(trigger_id);
    m_pending_trigger_decision_time_us += std::chrono::duration_cast<std::chrono::microseconds>(now - received).count();
    update_busy_status(lock);
    m_status_updated.store(true);
    m_status_update_cv.notify_all();
  }

  m_trigger_decision_sender->send(std::move(decision), m_td_queue_timeout);
  ++m_num_trigger_decisions_sent;
}

bool
DataflowStatusModule::process_trb_completion(const dfmessages::TRBCompletion& completion,
                                             std::chrono::steady_clock::time_point received)
{

  TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << " Processing TRBCompletion for trigger record "
                              << completion.trigger_id.trigger_number << "." << completion.sequence_number << "/"
                              << completion.trigger_id.run_number;
  {
    std::lock_guard<std::mutex> lock(m_status_mutex);

    auto now = std::chrono::steady_clock::now();
    if (!m_current_status.triggers_building.count(completion.trigger_id)) {
      if (now - received > m_pending_message_timeout) {
        ers::error(UnexpectedTRBCompletion(
          ERS_HERE, get_name(), completion.trigger_id.trigger_number, completion.trigger_id.run_number));
        m_num_pending_trb_completion_timeouts++;
        return true; // Remove from list
      }
      return false;
    }

    ++m_num_trb_completions_received;

    if (completion.trigger_record_max_sequence_number > 0) {
      if (m_trigger_statuses[completion.trigger_id].max_sequence_number == 0) {
        m_trigger_statuses[completion.trigger_id].max_sequence_number = completion.trigger_record_max_sequence_number;
      }
      m_trigger_statuses[completion.trigger_id].sequences_built++;

      if (m_trigger_statuses[completion.trigger_id].sequences_built ==
          m_trigger_statuses[completion.trigger_id].max_sequence_number + 1) {
        TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << " Received TRBCompletion for trigger record "
                                    << completion.trigger_id.trigger_number << "." << completion.sequence_number << "/"
                                    << completion.trigger_id.run_number << ". All sequences have been built.";
      } else {
        TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << " Received TRBCompletion for trigger record "
                                    << completion.trigger_id.trigger_number << "." << completion.sequence_number << "/"
                                    << completion.trigger_id.run_number << ". Total completed sequences: "
                                    << m_trigger_statuses[completion.trigger_id].sequences_built << " of "
                                    << m_trigger_statuses[completion.trigger_id].max_sequence_number + 1;
        return true;
      }
    }

    m_pending_trb_completion_time_us += std::chrono::duration_cast<std::chrono::microseconds>(now - received).count();
    m_current_status.triggers_building.erase(completion.trigger_id);
    m_current_status.triggers_writing.insert(completion.trigger_id);
    m_status_updated.store(true);
    m_status_update_cv.notify_all();
  }
  return true;
}

bool
DataflowStatusModule::process_trigger_decision_token(const dfmessages::TriggerDecisionToken& token,
                                                     std::chrono::steady_clock::time_point received)
{

  TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << " Received TriggerDecisionToken for trigger record "
                              << token.trigger_id.trigger_number << "." << token.sequence_number << "/"
                              << token.trigger_id.run_number;
  {
    std::unique_lock<std::mutex> lock(m_status_mutex);

    auto now = std::chrono::steady_clock::now();
    if (m_current_status.triggers_writing.count(token.trigger_id) == 0) {
      if (now - received > m_pending_message_timeout) {
        ++m_num_pending_trigger_decision_token_timeouts;
        if(m_trigger_statuses.count(token.trigger_id) == 0) {
        ers::error(UnexpectedTriggerDecisionToken(
          ERS_HERE, get_name(), token.trigger_id.trigger_number, m_current_status.trigger_id.run_number));
        return true; // Remove from list
        } else {
          ers::warning(UnexpectedTriggerDecisionToken(
            ERS_HERE, get_name(), token.trigger_id.trigger_number, m_current_status.trigger_id.run_number));
        }
      } else {
      return false;
      }
    }

    ++m_num_trigger_decision_tokens_received;

    if (m_trigger_statuses[token.trigger_id].max_sequence_number > 0) {
      m_trigger_statuses[token.trigger_id].sequences_written++;

      if (m_trigger_statuses[token.trigger_id].sequences_written ==
          m_trigger_statuses[token.trigger_id].max_sequence_number + 1) {
        TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << " Received TriggerDecisionToken for trigger record "
                                    << token.trigger_id.trigger_number << "." << token.sequence_number << "/"
                                    << token.trigger_id.run_number << ". All sequences have been written.";
      } else {
        TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << " Received TriggerDecisionToken for trigger record "
                                    << token.trigger_id.trigger_number << "." << token.sequence_number << "/"
                                    << token.trigger_id.run_number << ". Total completed sequences: "
                                    << m_trigger_statuses[token.trigger_id].sequences_written << " of "
                                    << m_trigger_statuses[token.trigger_id].max_sequence_number + 1;
        return true;
      }
    }

    m_pending_trigger_decision_token_time_us +=
      std::chrono::duration_cast<std::chrono::microseconds>(now - received).count();

    m_complete_counter++;
    uint64_t completion_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                    now - m_trigger_statuses[token.trigger_id].decision_received)
                                    .count();
    m_completion_time_us += completion_time_us;
    if (m_min_completion_time_us.load() == 0 || completion_time_us < m_min_completion_time_us.load()) {
      m_min_completion_time_us.store(completion_time_us);
    }
    if (completion_time_us > m_max_completion_time_us.load()) {
      m_max_completion_time_us.store(completion_time_us);
    }

    m_trigger_statuses.erase(token.trigger_id);
    m_current_status.triggers_building.erase(token.trigger_id);
    m_current_status.triggers_writing.erase(token.trigger_id);
    m_current_status.recently_completed_triggers.insert(token.trigger_id);

    while (m_current_status.recently_completed_triggers.size() > m_completed_trigger_history_size) {
      m_current_status.recently_completed_triggers.erase(m_current_status.recently_completed_triggers.begin());
    }

    m_current_status.trigger_records_processed++;
    m_current_status.data_size_written += token.data_size;
    update_busy_status(lock);

    m_status_updated.store(true);
    m_status_update_cv.notify_all();
  }

  return true;
}

void
DataflowStatusModule::send_dataflow_status_update(std::string const& destination,
                                                  dfmessages::trigger_number_t trigger_number)
{
  TLOG_DEBUG(TLVL_SEND_STATE) << get_name() << ": Sending DataflowStatus update to " << destination
                              << " for trigger number " << trigger_number;
  dfmessages::DataflowStatus status_to_send;
  {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    if (trigger_number == 0) {
      status_to_send = m_current_status;
    } else {
      auto it = m_status_snapshots.find({ m_current_status.trigger_id.run_number, trigger_number });
      if (it != m_status_snapshots.end()) {
        status_to_send = it->second;
      } else {
        ers::warning(SnapshotNotFound(ERS_HERE, get_name(), trigger_number));
        status_to_send = m_current_status;
      }
    }
  }

  status_to_send.trigger_id.trigger_number = trigger_number;
  TLOG_DEBUG(TLVL_SEND_STATE) << get_name() << ": Sending DataflowStatus, trigger_number: "
                              << status_to_send.trigger_id.trigger_number << ", is_busy: " << status_to_send.is_busy;
  auto iom = iomanager::IOManager::get();
  auto sender = iom->get_sender<dfmessages::DataflowStatus>(destination);
  if (sender) {
    try {
      sender->send(std::move(status_to_send), m_heartbeat_interval);
      ++m_num_status_messages_sent;
    } catch (iomanager::TimeoutExpired& e) {
      TLOG() << get_name() << ": Timeout expired while sending DataflowStatus update to " << destination
             << ". Error: " << e.what();
    }
  } else {
    TLOG() << get_name() << ": No sender found for destination " << destination
           << ". Unable to send DataflowStatus update.";
  }
}

void
DataflowStatusModule::status_update_thread(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering status_update_thread() method";
  while (running_flag.load()) {
    bool did_work = false;

    // Always accept incoming trigger decisions
    {
      std::lock_guard<std::mutex> lock(m_pending_trigger_decision_mutex);
      for (auto& decision_pair : m_pending_trigger_decisions) {
        did_work = true;
        process_trigger_decision(decision_pair.first, decision_pair.second);
      }
      m_pending_trigger_decisions.clear();
    }

    // TRB completions are only accepted if we have received a decision
    {
      std::lock_guard<std::mutex> lock(m_pending_trb_completion_mutex);
      std::erase_if(m_pending_trb_completions, [&](auto& completion_pair) {
        auto ret = process_trb_completion(completion_pair.first, completion_pair.second);
        did_work |= ret;
        return ret;
      });
    }

    // Only accept trigger decision tokens if we have seen a TRB Completion for the corresponding trigger decision
    {
      std::lock_guard<std::mutex> lock(m_pending_trigger_decision_token_mutex);
      std::erase_if(m_pending_trigger_decision_tokens, [&](auto& token_pair) {
        auto ret = process_trigger_decision_token(token_pair.first, token_pair.second);
        did_work |= ret;
        return ret;
      });
    }

    if (!did_work) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting status_update_thread() method";
}

void
DataflowStatusModule::status_heartbeat_thread(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering status_heartbeat_thread() method";
  while (running_flag.load()) {
    TLOG_DEBUG(TLVL_HEARTBEAT) << get_name() << ": Heartbeat thread waiting for " << m_heartbeat_interval.count()
                               << " ms or status update notification";
    {
      std::unique_lock<std::mutex> lk(m_status_mutex);
      m_status_update_cv.wait_for(
        lk, m_heartbeat_interval, [&]() { return !running_flag.load() || m_status_updated.load(); });
    }
    if (!running_flag.load()) {
      break;
    }
    TLOG_DEBUG(TLVL_HEARTBEAT) << get_name() << ": Heartbeat thread woke up. Sending status updates to "
                               << m_known_dfos.size() << " known DFOs.";
    for (const auto& dfo : m_known_dfos) {
      send_dataflow_status_update(dfo);
    }
    m_status_updated.store(false);
    TLOG_DEBUG(TLVL_HEARTBEAT) << get_name() << ": Heartbeat thread finished sending status updates.";
  }
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting status_heartbeat_thread() method";
}

void
DataflowStatusModule::update_busy_status(std::unique_lock<std::mutex>& lk)
{
  if (!lk.owns_lock()) {
    throw std::runtime_error("update_busy_status must be called with a unique_lock that owns the lock");
  }
  auto current_workload = m_current_status.triggers_building.size() + m_current_status.triggers_writing.size();
  if (m_current_status.is_busy && current_workload < m_current_status.free_threshold) {
    m_current_status.is_busy = false;
    TLOG() << get_name() << ": Transitioning to FREE status. Current workload: " << current_workload;
  }

  if (!m_current_status.is_busy && current_workload > m_current_status.busy_threshold) {
    m_current_status.is_busy = true;
    TLOG() << get_name() << ": Transitioning to BUSY status. Current workload: " << current_workload;
  }
}

} // namespace dunedaq::dfmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DataflowStatusModule)