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
#include "dfmessages/TriggerInhibit.hpp"
#include "logging/Logging.hpp"
#include "networkmanager/NetworkManager.hpp"

#include <chrono>
#include <cstdlib>
#include <future>
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
    m_dataflow_availability[app.decision_connection] = TriggerRecordBuilderData(app.decision_connection, 
										app.thresholds.busy, 
										app.thresholds.free);
  }
  //m_dataflow_availability_iter = m_dataflow_availability.begin();

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

  networkmanager::NetworkManager::get().register_callback(
    m_token_connection_name,
    std::bind(&DataFlowOrchestrator::receive_trigger_complete_token, this, std::placeholders::_1));

  networkmanager::NetworkManager::get().register_callback(
    m_td_connection_name,
    std::bind(&DataFlowOrchestrator::receive_trigger_decision, this, std::placeholders::_1));

  //m_working_thread.start_working_thread();

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DataFlowOrchestrator::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  //m_working_thread.stop_working_thread();
  m_running_status.store(true);

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

  TLOG() << get_name() << " successfully scrapped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

// void
// DataFlowOrchestrator::do_work(std::atomic<bool>& run_flag)
// {
//   std::chrono::steady_clock::time_point last_slot_check, slot_available, assignment_possible;

//   last_slot_check = std::chrono::steady_clock::now();
//   while (run_flag.load()) {
//     if (has_slot()) {
//       slot_available = std::chrono::steady_clock::now();
//       m_waiting_for_slots +=
//         std::chrono::duration_cast<std::chrono::microseconds>(slot_available - last_slot_check).count();
//       dfmessages::TriggerDecision decision;

//       bool has_decision = false;
//       while (!has_decision && run_flag.load()) {
//         has_decision = extract_a_decision(decision);
//         if (has_decision) {
//           assignment_possible = std::chrono::steady_clock::now();

//           m_waiting_for_decision +=
//             std::chrono::duration_cast<std::chrono::microseconds>(assignment_possible - slot_available).count();

//           while (run_flag.load()) {

//             auto assignment = find_slot(decision);

//             if (assignment == nullptr)
//               continue;

//             auto dispatch_successful = dispatch(assignment, run_flag);

//             if (dispatch_successful) {
//               assign_trigger_decision(assignment);
//               break;
//             } else {
//               ers::error(TriggerRecordBuilderAppUpdate(
//                 ERS_HERE, assignment->connection_name, "Could not send Trigger Decision"));
//               m_dataflow_availability[assignment->connection_name].set_in_error(true);
//             }
//           }

//           auto assignment_complete = std::chrono::steady_clock::now();

//           m_deciding_destination +=
//             std::chrono::duration_cast<std::chrono::microseconds>(assignment_complete - assignment_possible).count();
//           last_slot_check = assignment_complete; // We'll restart the loop next, so set the first time counter to ensure
//                                                  // that the three counters do not miss any time

//         } else { // failed at extracting the decisions
//           // Incrementally update waiting_for_decision time counter
//           auto failed_extracting_decision = std::chrono::steady_clock::now();
//           m_waiting_for_decision +=
//             std::chrono::duration_cast<std::chrono::microseconds>(failed_extracting_decision - slot_available).count();
//           slot_available = failed_extracting_decision; // For while loop continuity
//         }
//       }
//     } else { // no slots available
//       auto lk = std::unique_lock<std::mutex>(m_slot_available_mutex);
//       m_slot_available_cv.wait_for(lk, std::chrono::milliseconds(1), [&]() { return has_slot(); });

//       // Incrementally update waiting_for_slots time counter
//       auto no_slots_available_time = std::chrono::steady_clock::now();
//       m_waiting_for_slots +=
//         std::chrono::duration_cast<std::chrono::microseconds>(no_slots_available_time - last_slot_check).count();
//       last_slot_check = no_slots_available_time; // Set this here so we don't miss time going around the loop
//     }
//   }
//   dfmessages::TriggerDecision decision;
//   while (extract_a_decision(decision)) {
//     auto assignment = find_slot(decision);
//     dispatch(assignment, run_flag);
//   }
// }

void
DataFlowOrchestrator::receive_trigger_decision(ipm::Receiver::Response message)
{
  //  std::chrono::steady_clock::time_point last_slot_check, slot_available, assignment_possible;

  //  last_slot_check = std::chrono::steady_clock::now();

  dfmessages::TriggerDecision decision;
  try {
    decision = serialization::deserialize<dfmessages::TriggerDecision>(message.data);
  } catch (const ers::Issue& excpt) {
    ers::error(excpt);
  }

  do {
    
    auto assignment = find_slot(decision);
    
    if (assignment == nullptr)  // this can happen if all application are in error state
      continue;
    
    auto dispatch_successful = dispatch(assignment);

    if (dispatch_successful) {
      assign_trigger_decision(assignment);
      break;
    } else {
      ers::error(TriggerRecordBuilderAppUpdate( ERS_HERE, 
						assignment->connection_name, 
						"Could not send Trigger Decision"));
      m_dataflow_availability[assignment->connection_name].set_in_error(true);
    }
    
  } while ( m_running_status.load() );

  notify_trigger(is_busy());
  
}


std::shared_ptr<AssignedTriggerDecision>
DataFlowOrchestrator::find_slot(dfmessages::TriggerDecision decision)
{

  // this find_slot assigns to the application with the lowest occupancy 
  // with respect to the busy threshold
  // application in error state are remove from the choice

  std::shared_ptr<AssignedTriggerDecision> output = nullptr;

  auto candidate = m_dataflow_availability.end() ;
  double ratio = std::numeric_limits<double>::max() ;
  for ( auto it = m_dataflow_availability.begin();
	it != m_dataflow_availability.end(); ++it ) {
    const auto & data = it->second;
    if ( ! data.is_in_error() ) {
      double temp_ratio = data.used_slots() / (double) data.busy_threshold() ;
      if ( temp_ratio < ratio ) {
	candidate = it ;
	ratio = temp_ratio;
      }
    }
  }

  if ( candidate != m_dataflow_availability.end() )
    output = candidate->second.make_assignment(decision);
  
  return output;
}

void
DataFlowOrchestrator::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  datafloworchestratorinfo::Info info;
  info.tokens_received = m_received_tokens.exchange(0);
  info.decisions_sent = m_sent_decisions.exchange(0);
  info.decisions_received = m_received_decisions.exchange(0);
  info.deciding_destination = m_deciding_destination.exchange(0);
  info.waiting_for_decision = m_waiting_for_decision.exchange(0);
  info.waiting_for_slots = m_waiting_for_slots.exchange(0);
  ci.add(info);
}

void
DataFlowOrchestrator::receive_trigger_complete_token(ipm::Receiver::Response message)
{
  auto token = serialization::deserialize<dfmessages::TriggerDecisionToken>(message.data);
  ++m_received_tokens;

  // add a check to see if the application data found
  if (token.run_number != m_run_number) 
    return ;  // should we print an error?
  
  auto app_it = m_dataflow_availability.find(token.decision_destination);
  // check if application data exists;
  if ( app_it == m_dataflow_availability.end() )  
    return;

  try {
    app_it -> second.complete_assignment(token.trigger_number, m_metadata_function);
    
  } catch (AssignedTriggerDecisionNotFound const& err) {
    ers::warning(err);
  }
  
  if ( app_it -> second.is_in_error()) {
    TLOG() << TriggerRecordBuilderAppUpdate(ERS_HERE, token.decision_destination, "Has reconnected");
    app_it -> second.set_in_error(false);
  }
  
  if ( ! app_it -> second.is_busy() ) {
    notify_trigger(false);
  }

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
DataFlowOrchestrator::notify_trigger(bool busy) const {

  auto message = dunedaq::serialization::serialize(dfmessages::TriggerInhibit{busy, m_run_number},
						   dunedaq::serialization::kMsgPack);

  bool wasSentSuccessfully = false;

  do {
    
    try {
      networkmanager::NetworkManager::get().send_to(m_busy_connection_name,
                                                    static_cast<const void*>(message.data()),
                                                    message.size(),
                                                    m_queue_timeout);
      wasSentSuccessfully = true;
    } catch (const ers::Issue& excpt) {
      std::ostringstream oss_warn;
      oss_warn << "Send to connection \"" << m_busy_connection_name << "\" failed";
      ers::warning(networkmanager::OperationFailed(ERS_HERE, oss_warn.str(), excpt));
    }

  } while (!wasSentSuccessfully && m_running_status.load() );

}

bool
DataFlowOrchestrator::dispatch(std::shared_ptr<AssignedTriggerDecision> assignment)
{

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering dispatch() method";
  auto serialised_decision = dunedaq::serialization::serialize(assignment->decision, 
							       dunedaq::serialization::kMsgPack);

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
