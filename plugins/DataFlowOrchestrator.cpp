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

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/app/Nljs.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"
#include "logging/Logging.hpp"
#include "networkmanager/NetworkManager.hpp"

#include <chrono>
#include <cstdlib>
#include <future>
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
  , m_working_thread(std::bind(&DataFlowOrchestrator::do_work, this, std::placeholders::_1))
{
  register_command("conf", &DataFlowOrchestrator::do_conf);
  register_command("start", &DataFlowOrchestrator::do_start);
  register_command("stop", &DataFlowOrchestrator::do_stop);
  register_command("scrap", &DataFlowOrchestrator::do_scrap);
}

void
DataFlowOrchestrator::init(const data_t& init_data)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  //----------------------
  // Get queue
  //----------------------

  auto qi = appfwk::queue_index(init_data, { "trigger_decision_queue" });

  try {
    auto temp_info = qi["trigger_decision_queue"];
    std::string temp_name = temp_info.inst;
    m_trigger_decision_queue.reset(new triggerdecisionsource_t(temp_name));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_input_queue", excpt);
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DataFlowOrchestrator::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  datafloworchestrator::ConfParams parsed_conf = payload.get<datafloworchestrator::ConfParams>();

  for (auto& app : parsed_conf.dataflow_applications) {
    m_dataflow_availability[app.decision_connection] = TriggerRecordBuilderData(app.decision_connection, app.capacity);
  }

  m_queue_timeout = std::chrono::milliseconds(parsed_conf.general_queue_timeout);
  m_token_connection_name = parsed_conf.token_connection;
  m_td_send_retries = parsed_conf.td_send_retries;

  networkmanager::NetworkManager::get().start_listening(m_token_connection_name);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
DataFlowOrchestrator::do_start(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  m_received_tokens = 0;
  m_run_number = payload.value<dunedaq::daqdataformats::run_number_t>("run", 0);

  networkmanager::NetworkManager::get().register_callback(
    m_token_connection_name,
    std::bind(&DataFlowOrchestrator::receive_trigger_complete_token, this, std::placeholders::_1));

  m_working_thread.start_working_thread();

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DataFlowOrchestrator::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  m_working_thread.stop_working_thread();

  networkmanager::NetworkManager::get().clear_callback(m_token_connection_name);

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DataFlowOrchestrator::do_scrap(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  networkmanager::NetworkManager::get().stop_listening(m_token_connection_name);

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
DataFlowOrchestrator::do_work(std::atomic<bool>& run_flag)
{
  std::chrono::steady_clock::time_point slot_available, assignment_possible;
  auto assignment_complete = std::chrono::steady_clock::now();

  while (run_flag.load()) {
    if (has_slot()) {
      slot_available = std::chrono::steady_clock::now();
      dfmessages::TriggerDecision decision;
      auto res = extract_a_decision(decision, run_flag);
      if (res) {
        assignment_possible = std::chrono::steady_clock::now();
        while (run_flag.load()) {

          auto assignment = find_slot(decision);
          auto dispatch_successful = dispatch(assignment, run_flag);

          if (dispatch_successful) {
            assign_trigger_decision(assignment);
            break;
          } else {
            ers::error(
              TriggerRecordBuilderAppUpdate(ERS_HERE, assignment->connection_name, "Could not send Trigger Decision"));
            m_dataflow_availability[assignment->connection_name].set_in_error(true);
          }
        }
        m_dataflow_busy +=
          std::chrono::duration_cast<std::chrono::microseconds>(slot_available - assignment_complete).count();
        assignment_complete = std::chrono::steady_clock::now();
        m_waiting_for_decision +=
          std::chrono::duration_cast<std::chrono::microseconds>(assignment_possible - slot_available).count();
        m_dfo_busy +=
          std::chrono::duration_cast<std::chrono::microseconds>(assignment_complete - assignment_possible).count();
      }
    } else {
      auto lk = std::unique_lock<std::mutex>(m_slot_available_mutex);
      m_slot_available_cv.wait_for(lk, std::chrono::milliseconds(1), [&]() { return has_slot(); });
    }
  }
  dfmessages::TriggerDecision decision;
  while (extract_a_decision(decision, run_flag)) {
    auto assignment = find_slot(decision);
    dispatch(assignment, run_flag);
  }
}

std::shared_ptr<AssignedTriggerDecision>
DataFlowOrchestrator::find_slot(dfmessages::TriggerDecision decision)
{
  static std::map<std::string, TriggerRecordBuilderData>::iterator last_selected_trb = m_dataflow_availability.begin();

  std::shared_ptr<AssignedTriggerDecision> output = nullptr;

  while (output == nullptr) {
    ++last_selected_trb;
    if (last_selected_trb == m_dataflow_availability.end())
      last_selected_trb = m_dataflow_availability.begin();

    if (last_selected_trb->second.has_slot()) {
      output = last_selected_trb->second.make_assignment(decision);
    }
  }

  return output;
}

void
DataFlowOrchestrator::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  datafloworchestratorinfo::Info info;
  info.tokens_received = m_received_tokens.exchange(0);
  info.decisions_sent = m_sent_decisions.exchange(0);
  info.decisions_received = m_received_decisions.exchange(0);
  info.dataflow_busy = m_dataflow_busy.exchange(0);
  info.waiting_for_decision = m_waiting_for_decision.exchange(0);
  info.dfo_busy = m_dfo_busy.exchange(0);
  ci.add(info);
}

void
DataFlowOrchestrator::receive_trigger_complete_token(ipm::Receiver::Response message)
{
  auto token = serialization::deserialize<dfmessages::TriggerDecisionToken>(message.data);
  ++m_received_tokens;

  if (token.run_number == m_run_number) {
    m_dataflow_availability[token.decision_destination].complete_assignment(token.trigger_number, m_metadata_function);

    if (m_dataflow_availability[token.decision_destination].is_in_error()) {
      TLOG() << TriggerRecordBuilderAppUpdate(ERS_HERE, token.decision_destination, "Has reconnected");
      m_dataflow_availability[token.decision_destination].set_in_error(false);
    }
    m_slot_available_cv.notify_all();
  }
}

bool
DataFlowOrchestrator::has_slot() const
{
  for (auto& dfapp : m_dataflow_availability) {
    if (dfapp.second.has_slot())
      return true;
  }
  return false;
}

bool
DataFlowOrchestrator::extract_a_decision(dfmessages::TriggerDecision& decision, std::atomic<bool>& run_flag)
{
  bool got_something = false;

  do {
    try {
      m_trigger_decision_queue->pop(decision, m_queue_timeout);
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Popped the Trigger Decision with number "
                                  << decision.trigger_number << " off the input queue";
      got_something = true;
      ++m_received_decisions;
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      // it is perfectly reasonable that there might be no data in the queue
      // some fraction of the times that we check, so we just continue on and try again
      continue;
    }

  } while (!got_something && run_flag.load());

  return got_something;
}
bool
DataFlowOrchestrator::dispatch(std::shared_ptr<AssignedTriggerDecision> assignment, std::atomic<bool>& run_flag)
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

  } while (!wasSentSuccessfully && run_flag.load() && retries > 0);

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
