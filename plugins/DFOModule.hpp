/**
 * @file DFOModule.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_
#define DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_

#include "dfmodules/AssignedTriggerDecision.hpp"
#include "dfmodules/DFOTriggerCounter.hpp"
#include "dfmodules/ReceivedDataflowStatus.hpp"

#include "appmodel/DFOConf.hpp"

#include "dfmessages/DataflowStatus.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/TriggerInhibit.hpp"

#include "iomanager/Sender.hpp"

#include "appfwk/DAQModule.hpp"
#include "logging/Logging.hpp" // NOTE: if ISSUES ARE DECLARED BEFORE include logging/Logging.hpp, TLOG_DEBUG<<issue wont work.

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace dunedaq {

// Disable coverage checking LCOV_EXCL_START
ERS_DECLARE_ISSUE_BASE(dfmodules,
                       TRBModuleAppUpdate,
                       appfwk::GeneralDAQModuleIssue,
                       "TRBModule app " << connection_name << ": " << message,
                       ((std::string)name),
                       ((std::string)connection_name)((std::string)message))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       UnknownTokenSource,
                       appfwk::GeneralDAQModuleIssue,
                       "Token from unknown source: " << connection_name,
                       ((std::string)name),
                       ((std::string)connection_name))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       DFOModuleRunNumberMismatch,
                       appfwk::GeneralDAQModuleIssue,
                       "DFOModule encountered run number mismatch: recvd ("
                         << received_run_number << ") != " << run_number << " from " << src_app
                         << " for trigger_number " << trig_num,
                       ((std::string)name),
                       ((uint32_t)received_run_number)((uint32_t)run_number)((std::string)src_app)(
                         (uint32_t)trig_num)) // NOLINT(build/unsigned)

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       IncompleteTriggerDecision,
                       appfwk::GeneralDAQModuleIssue,
                       "TriggerDecision " << trigger_number << " didn't complete within timeout in run " << run_number,
                       ((std::string)name),
                       ((uint32_t)trigger_number)((uint32_t)run_number)) // NOLINT(build/unsigned)

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       UnexpectedTriggerDecision,
                       appfwk::GeneralDAQModuleIssue,
                       "TriggerDecision " << trigger_number << " has been reported by " << app
                                          << " with no TriggerDecision message received",
                       ((std::string)name),
                       ((uint32_t)trigger_number)((std::string)app)) // NOLINT(build/unsigned)

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       LostTriggerDecision,
                       appfwk::GeneralDAQModuleIssue,
                       "TriggerDecision " << trigger_number << " was lost while being processed by " << app,
                       ((std::string)name),
                       ((uint32_t)trigger_number)((std::string)app)) // NOLINT(build/unsigned)

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       UnableToAssign,
                       appfwk::GeneralDAQModuleIssue,
                       "TriggerDecision " << trigger_number << " could not be assigned",
                       ((std::string)name),
                       ((uint32_t)trigger_number)) // NOLINT(build/unsigned)

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       AssignedToBusyApp,
                       appfwk::GeneralDAQModuleIssue,
                       "TriggerDecision " << trigger_number << " was assigned to DF app " << app
                                          << " that was busy with " << used_slots << " TDs",
                       ((std::string)name),
                       ((uint32_t)trigger_number)((std::string)app)((size_t)used_slots)) // NOLINT(build/unsigned)

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       StaleDataflowStatus,
                       appfwk::GeneralDAQModuleIssue,
                       "No DataflowStatus received from " << app << " for " << timeout << " ms",
                       ((std::string)name),
                       ((std::string)app)((uint32_t)timeout)) // NOLINT(build/unsigned)

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       ReallocatingTrigger,
                       appfwk::GeneralDAQModuleIssue,
                       "Reallocating trigger " << trigger << " from DF app " << app,
                       ((std::string)name),
                       ((uint32_t)trigger)((std::string)app)) // NOLINT(build/unsigned)
// Re-enable coverage checking LCOV_EXCL_STOP

namespace dfmodules {

/**
 * @brief DFOModule distributes triggers according to the
 * availability of the DF apps in the system
 */
class DFOModule : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief DFOModule Constructor
   * @param name Instance name for this DFOModule instance
   */
  explicit DFOModule(const std::string& name);

  DFOModule(const DFOModule&) = delete;            ///< DFOModule is not copy-constructible
  DFOModule& operator=(const DFOModule&) = delete; ///< DFOModule is not copy-assignable
  DFOModule(DFOModule&&) = delete;                 ///< DFOModule is not move-constructible
  DFOModule& operator=(DFOModule&&) = delete;      ///< DFOModule is not move-assignable

  void init(std::shared_ptr<appfwk::ConfigurationManager> mcfg) override;

private:
  // Commands
  void do_conf(const CommandData_t&);
  void do_start(const CommandData_t&);
  void do_stop(const CommandData_t&);
  void do_scrap(const CommandData_t&);

  void generate_opmon_data() override;

  // Configuration
  const appmodel::DFOConf* m_dfo_conf;
  std::chrono::milliseconds m_queue_timeout;
  std::chrono::microseconds m_stop_timeout;
  std::chrono::milliseconds m_request_reply_wait;
  std::chrono::milliseconds m_status_watchdog_interval;
  std::chrono::milliseconds m_dataflow_status_timeout;
  dunedaq::daqdataformats::run_number_t m_run_number;
  bool m_reallocate_building_triggers_on_timeout{ false };
  bool m_reallocate_writing_triggers_on_timeout{ false };

  // Connections
  std::shared_ptr<iomanager::SenderConcept<dfmessages::TriggerInhibit>> m_busy_sender;
  std::string m_status_connection;
  std::string m_td_connection;
  std::vector<std::string> m_trb_conn_ids;
  size_t m_td_send_retries;

  void receive_dataflow_status(const dfmessages::DataflowStatus&);
  void receive_trigger_decision(const dfmessages::TriggerDecision&);
  void notify_trigger_if_needed() const;

  bool send_status_requests(dfmessages::trigger_number_t trigger, size_t iteration);
  bool dispatch(const std::shared_ptr<AssignedTriggerDecision>& assignment);

  // Dataflow application selection algorithm
  std::shared_ptr<AssignedTriggerDecision> find_slot(const dfmessages::TriggerDecision& decision);
  void assign_trigger_decision(const std::shared_ptr<AssignedTriggerDecision>& assignment);

  // Coordination

  std::mutex m_status_mutex;
  std::condition_variable m_status_cv;
  std::unordered_map<std::string, std::shared_ptr<ReceivedDataflowStatus>> m_dataflow_statuses;
  std::unordered_map<dfmessages::trigger_number_t, std::unordered_map<std::string, dfmessages::DataflowStatus>>
    m_statuses_for_trigger;
  std::unordered_map<dfmessages::trigger_number_t, std::shared_ptr<AssignedTriggerDecision>>
    m_assigned_trigger_decisions;

  std::atomic<bool> m_running_status{ false };
  mutable std::atomic<bool> m_last_notified_busy{ false };
  std::atomic<bool> m_processing_td{ false };
  std::chrono::steady_clock::time_point m_last_td_received;
  mutable std::mutex m_notify_trigger_mutex;
  std::shared_ptr<std::jthread> m_status_watchdog_thread;
  std::unordered_map<dfmessages::trigger_number_t, std::shared_ptr<std::jthread>> m_decision_assignment_threads;

  void status_watchdog_proc(std::stop_token stoken);
  bool is_busy() const;

  // Statistics
  std::atomic<uint64_t> m_received_statuses{ 0 };    // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_sent_decisions{ 0 };       // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_received_decisions{ 0 };   // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_completed_decisions{ 0 };  // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_waiting_for_decision{ 0 }; // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_deciding_destination{ 0 }; // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_forwarding_decision{ 0 };  // NOLINT (build/unsigned)
  std::map<dunedaq::trgdataformats::TriggerCandidateData::Type, DFOTriggerCounter> m_trigger_counters;
  std::mutex m_trigger_counters_mutex; // used to safely handle the map above
  DFOTriggerCounter& get_trigger_counter(trgdataformats::TriggerCandidateData::Type type);
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_
