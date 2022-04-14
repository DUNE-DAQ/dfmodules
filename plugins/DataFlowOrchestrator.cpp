/**
 * @file DataFlowOrchestrator.cpp DataFlowOrchestrator class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DataFlowOrchestrator.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "dfmodules/datafloworchestrator/Nljs.hpp"
#include "dfmodules/datafloworchestratorinfo/InfoNljs.hpp"
#include "dfmodules/dfapplicationinfo/InfoNljs.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/app/Nljs.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TriggerInhibit.hpp"
#include "logging/Logging.hpp"
#include "networkmanager/NetworkManager.hpp"

#include <chrono>
#include <cstdlib>
#include <future>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "DataFlowOrchestrator" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10
};

namespace dunedaq {
namespace dfmodules {

DataFlowOrchestrator::DataFlowOrchestrator(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_queue_timeout(100)
  , m_run_number(0)
{
  register_command("conf", &DataFlowOrchestrator::do_conf);
  register_command("start", &DataFlowOrchestrator::do_start);
  register_command("stop", &DataFlowOrchestrator::do_stop);
  register_command("scrap", &DataFlowOrchestrator::do_scrap);
}

void
DataFlowOrchestrator::init(const data_t& /*init_data*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  //----------------------
  // the module has no queues now
  //----------------------

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DataFlowOrchestrator::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  datafloworchestrator::ConfParams parsed_conf = payload.get<datafloworchestrator::ConfParams>();

  for (auto& app : parsed_conf.dataflow_applications) {
    m_dataflow_availability[app.decision_connection] =
      TriggerRecordBuilderData(app.decision_connection, app.thresholds.busy, app.thresholds.free);
    m_app_infos[app.decision_connection]; // we just need to create the object
  }

  m_queue_timeout = std::chrono::milliseconds(parsed_conf.general_queue_timeout);
  m_token_connection_name = parsed_conf.token_connection;
  m_busy_connection_name = parsed_conf.busy_connection;
  m_td_connection_name = parsed_conf.td_connection;

  m_td_send_retries = parsed_conf.td_send_retries;

  networkmanager::NetworkManager::get().start_listening(m_token_connection_name);
  networkmanager::NetworkManager::get().start_listening(m_td_connection_name);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method, there are "
                                      << m_dataflow_availability.size() << " TRB apps defined";
}

void
DataFlowOrchestrator::do_start(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  m_received_tokens = 0;
  m_run_number = payload.value<dunedaq::daqdataformats::run_number_t>("run", 0);

  m_running_status.store(true);
  m_last_notified_busy.store(false);
  m_last_assignement_it = m_dataflow_availability.end();

  m_last_token_received = m_last_td_received = std::chrono::steady_clock::now();

  networkmanager::NetworkManager::get().register_callback(
    m_token_connection_name,
    std::bind(&DataFlowOrchestrator::receive_trigger_complete_token, this, std::placeholders::_1));

  networkmanager::NetworkManager::get().register_callback(
    m_td_connection_name, std::bind(&DataFlowOrchestrator::receive_trigger_decision, this, std::placeholders::_1));

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DataFlowOrchestrator::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  m_running_status.store(false);

  networkmanager::NetworkManager::get().clear_callback(m_td_connection_name);
  networkmanager::NetworkManager::get().clear_callback(m_token_connection_name);

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DataFlowOrchestrator::do_scrap(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  networkmanager::NetworkManager::get().stop_listening(m_token_connection_name);
  networkmanager::NetworkManager::get().stop_listening(m_td_connection_name);

  m_dataflow_availability.clear();
  m_app_infos.clear();

  TLOG() << get_name() << " successfully scrapped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
DataFlowOrchestrator::receive_trigger_decision(ipm::Receiver::Response message)
{
  dfmessages::TriggerDecision decision;
  try {
    decision = serialization::deserialize<dfmessages::TriggerDecision>(message.data);
  } catch (const ers::Issue& excpt) {
    ers::error(excpt);
  }

  if (decision.run_number != m_run_number) {
    ers::warning(DataFlowOrchestratorRunNumberMismatch(ERS_HERE, decision.run_number, m_run_number, "MLT"));
    return;
  }

  ++m_received_decisions;
  auto decision_received = std::chrono::steady_clock::now();

  std::chrono::steady_clock::time_point decision_assigned;
  do {

    auto assignment = find_slot(decision);

    if (assignment == nullptr) // this can happen if all application are in error state
      continue;

    decision_assigned = std::chrono::steady_clock::now();
    auto dispatch_successful = dispatch(assignment);

    if (dispatch_successful) {
      assign_trigger_decision(assignment);
      break;
    } else {
      ers::error(
        TriggerRecordBuilderAppUpdate(ERS_HERE, assignment->connection_name, "Could not send Trigger Decision"));
      m_dataflow_availability[assignment->connection_name].set_in_error(true);
    }

  } while (m_running_status.load());

  notify_trigger(is_busy());

  m_waiting_for_decision +=
    std::chrono::duration_cast<std::chrono::microseconds>(decision_received - m_last_td_received).count();
  m_last_td_received = std::chrono::steady_clock::now();
  m_deciding_destination +=
    std::chrono::duration_cast<std::chrono::microseconds>(decision_assigned - decision_received).count();
  m_forwarding_decision +=
    std::chrono::duration_cast<std::chrono::microseconds>(m_last_td_received - decision_assigned).count();
}

std::shared_ptr<AssignedTriggerDecision>
DataFlowOrchestrator::find_slot(dfmessages::TriggerDecision decision)
{

  // this find_slot assings the decision with a round-robin logic
  // across all the available applications.
  // Applications in error are skipped.

  std::shared_ptr<AssignedTriggerDecision> output = nullptr;

  auto last_it = m_last_assignement_it;
  if (last_it == m_dataflow_availability.end())
    last_it = m_dataflow_availability.begin();

  auto candidate_it = last_it;

  do {

    ++candidate_it;
    if (candidate_it == m_dataflow_availability.end())
      candidate_it = m_dataflow_availability.begin();

    if (candidate_it->second.is_in_error())
      continue;

    output = candidate_it->second.make_assignment(decision);
    m_last_assignement_it = candidate_it;
    break;

  } while (candidate_it != last_it);

  return output;
}

void
DataFlowOrchestrator::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  datafloworchestratorinfo::Info info;
  info.tokens_received = m_received_tokens.exchange(0);
  info.decisions_sent = m_sent_decisions.exchange(0);
  info.decisions_received = m_received_decisions.exchange(0);
  info.waiting_for_decision = m_waiting_for_decision.exchange(0);
  info.deciding_destination = m_deciding_destination.exchange(0);
  info.forwarding_decision = m_forwarding_decision.exchange(0);
  info.waiting_for_token = m_waiting_for_token.exchange(0);
  info.processing_token = m_processing_token.exchange(0);
  ci.add(info);

  for (auto& [name, data] : m_app_infos) {
    dfapplicationinfo::Info tmp_info;
    tmp_info.outstanding_decisions = m_dataflow_availability[name].used_slots();
    tmp_info.completed_trigger_records = data.first.exchange(0);
    tmp_info.waiting_time = data.second.exchange(0);

    opmonlib::InfoCollector tmp_ic;
    tmp_ic.add(tmp_info);

    ci.add(name, tmp_ic);
  }
}

void
DataFlowOrchestrator::receive_trigger_complete_token(ipm::Receiver::Response message)
{
  auto token = serialization::deserialize<dfmessages::TriggerDecisionToken>(message.data);
  ++m_received_tokens;

  // add a check to see if the application data found
  if (token.run_number != m_run_number) {
    ers::warning(
      DataFlowOrchestratorRunNumberMismatch(ERS_HERE, token.run_number, m_run_number, token.decision_destination));
    return;
  }

  auto app_it = m_dataflow_availability.find(token.decision_destination);
  // check if application data exists;
  if (app_it == m_dataflow_availability.end())
    return;

  auto callback_start = std::chrono::steady_clock::now();

  try {
    auto dec_ptr = app_it->second.complete_assignment(token.trigger_number, m_metadata_function);

    auto& info_data = m_app_infos[app_it->first];
    ++info_data.first;
    info_data.second +=
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - dec_ptr->assigned_time)
        .count();

  } catch (AssignedTriggerDecisionNotFound const& err) {
    ers::warning(err);
  }

  if (app_it->second.is_in_error()) {
    TLOG() << TriggerRecordBuilderAppUpdate(ERS_HERE, token.decision_destination, "Has reconnected");
    app_it->second.set_in_error(false);
  }

  if (!app_it->second.is_busy()) {
    notify_trigger(false);
  }

  m_waiting_for_token +=
    std::chrono::duration_cast<std::chrono::microseconds>(callback_start - m_last_token_received).count();
  m_last_token_received = std::chrono::steady_clock::now();
  m_processing_token +=
    std::chrono::duration_cast<std::chrono::microseconds>(m_last_token_received - callback_start).count();
}

bool
DataFlowOrchestrator::is_busy() const
{
  for (auto& dfapp : m_dataflow_availability) {
    if (!dfapp.second.is_busy())
      return false;
  }
  return true;
}

void
DataFlowOrchestrator::notify_trigger(bool busy) const
{

  if (busy == m_last_notified_busy.load())
    return;

  auto message = dunedaq::serialization::serialize(dfmessages::TriggerInhibit{ busy, m_run_number },
                                                   dunedaq::serialization::kMsgPack);

  bool wasSentSuccessfully = false;

  do {

    try {
      networkmanager::NetworkManager::get().send_to(
        m_busy_connection_name, static_cast<const void*>(message.data()), message.size(), m_queue_timeout);
      wasSentSuccessfully = true;
    } catch (const ers::Issue& excpt) {
      std::ostringstream oss_warn;
      oss_warn << "Send to connection \"" << m_busy_connection_name << "\" failed";
      ers::warning(networkmanager::OperationFailed(ERS_HERE, oss_warn.str(), excpt));
    }

  } while (!wasSentSuccessfully && m_running_status.load());

  m_last_notified_busy.store(busy);
}

bool
DataFlowOrchestrator::dispatch(std::shared_ptr<AssignedTriggerDecision> assignment)
{

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering dispatch() method";
  auto serialised_decision = dunedaq::serialization::serialize(assignment->decision, dunedaq::serialization::kMsgPack);

  bool wasSentSuccessfully = false;
  int retries = m_td_send_retries;
  do {

    try {
      networkmanager::NetworkManager::get().send_to(assignment->connection_name,
                                                    static_cast<const void*>(serialised_decision.data()),
                                                    serialised_decision.size(),
                                                    m_queue_timeout);
      wasSentSuccessfully = true;
      ++m_sent_decisions;
    } catch (const ers::Issue& excpt) {
      std::ostringstream oss_warn;
      oss_warn << "Send to connection \"" << assignment->connection_name << "\" failed";
      ers::warning(networkmanager::OperationFailed(ERS_HERE, oss_warn.str(), excpt));
    }

    retries--;

  } while (!wasSentSuccessfully && m_running_status.load() && retries > 0);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting dispatch() method";
  return wasSentSuccessfully;
}

void
DataFlowOrchestrator::assign_trigger_decision(std::shared_ptr<AssignedTriggerDecision> assignment)
{
  m_dataflow_availability[assignment->connection_name].add_assignment(assignment);
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DataFlowOrchestrator)
