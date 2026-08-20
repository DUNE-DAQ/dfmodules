/**
 * @file DataflowStatusModule.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_DATAFLOWSTATUSMODULE_HPP_
#define DFMODULES_PLUGINS_DATAFLOWSTATUSMODULE_HPP_

#include "appfwk/DAQModule.hpp"
#include "appmodel/DataflowStatusModuleConf.hpp"
#include "dfmessages/DataflowStatus.hpp"
#include "dfmessages/DataflowStatusRequest.hpp"
#include "dfmessages/TRBCompletion.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"
#include "utilities/WorkerThread.hpp"

#include <atomic>
#include <memory>
#include <set>
#include <string>

namespace dunedaq {

// Disable coverage checking LCOV_EXCL_START

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       TriggerDecisionIncorrectRun,
                       appfwk::GeneralDAQModuleIssue,
                       "Received TriggerDecision message for trigger number "
                         << trigger_number << " in run " << run_number << " which is not the current run (" << current_run << ")",
                       ((std::string)name),
                       ((uint32_t)trigger_number)((uint32_t)run_number)((uint32_t)current_run))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       UnexpectedTRBCompletion,
                       appfwk::GeneralDAQModuleIssue,
                       "Received TRBCompletion message for trigger number "
                         << trigger_number << " in run " << run_number << " which is not in the building list",
                       ((std::string)name),
                       ((uint32_t)trigger_number)((uint32_t)run_number))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       UnexpectedTriggerDecisionToken,
                       appfwk::GeneralDAQModuleIssue,
                       "Received TriggerDecisionToken message for trigger number "
                         << trigger_number << " in run " << run_number << " which is not in the writing list",
                       ((std::string)name),
                       ((uint32_t)trigger_number)((uint32_t)run_number))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       SnapshotNotFound,
                       appfwk::GeneralDAQModuleIssue,
                       "No snapshot found for trigger number " << trigger_number << ". Sending current status instead.",
                       ((std::string)name),
                       ((uint32_t)trigger_number))
// Re-enable coverage checking LCOV_EXCL_STOP
namespace dfmodules {

class DataflowStatusModule : public dunedaq::appfwk::DAQModule
{
public:
  explicit DataflowStatusModule(const std::string& name);
  virtual ~DataflowStatusModule();

  DataflowStatusModule(const DataflowStatusModule&) = delete; ///< DataflowStatusModule is not copy-constructible
  DataflowStatusModule& operator=(const DataflowStatusModule&) =
    delete;                                                         ///< DataflowStatusModule is not copy-assignable
  DataflowStatusModule(DataflowStatusModule&&) = delete;            ///< DataflowStatusModule is not move-constructible
  DataflowStatusModule& operator=(DataflowStatusModule&&) = delete; ///< DataflowStatusModule is not move-assignable

  void init(std::shared_ptr<appfwk::ConfigurationManager> mcfg) override;
  void generate_opmon_data() override;

private:
  // Commands
  void do_conf(const CommandData_t&);
  void do_start(const CommandData_t&);
  void do_stop(const CommandData_t&);
  void do_scrap(const CommandData_t&);

  // Callback
  void receive_status_request(const dfmessages::DataflowStatusRequest&);
  void receive_trigger_decision(dfmessages::TriggerDecision&);
  void receive_trb_completion(const dfmessages::TRBCompletion&);
  void receive_trigger_decision_token(const dfmessages::TriggerDecisionToken&);

  // Configuration
  const appmodel::DataflowStatusModuleConf* m_conf;
  std::chrono::milliseconds m_heartbeat_interval{ 100 };
  std::chrono::milliseconds m_stop_timeout{ 1000 };
  std::chrono::milliseconds m_td_queue_timeout{ 1000 };
  size_t m_snapshot_history_size{ 100 };
  size_t m_completed_trigger_history_size{ 100 };

  // Connections
  std::shared_ptr<iomanager::SenderConcept<dfmessages::TriggerDecision>> m_trigger_decision_sender;
  std::string m_status_request_connection;
  std::string m_td_connection;
  std::string m_trb_completion_connection;
  std::string m_token_connection;
  std::set<std::string> m_known_dfos;

  // Status
  dfmessages::DataflowStatus m_current_status;
  std::map<dfmessages::TriggerId, dfmessages::DataflowStatus> m_status_snapshots;
  std::map<dfmessages::TriggerId, std::pair<dfmessages::sequence_number_t, dfmessages::sequence_number_t>>
    m_building_sequences;
  std::map<dfmessages::TriggerId, std::pair<dfmessages::sequence_number_t, dfmessages::sequence_number_t>>
    m_writing_sequences;
  std::mutex m_status_mutex;
  std::atomic<bool> m_status_updated;
  std::condition_variable m_status_update_cv;
  dunedaq::utilities::WorkerThread m_heartbeat_thread;
  void send_dataflow_status_update(std::string const& destination, dfmessages::trigger_number_t trigger_number = 0);
  void status_heartbeat_thread(std::atomic<bool>& running_flag);
  void update_busy_status(std::unique_lock<std::mutex>& lk);

  // Counters
  std::atomic<size_t> m_num_trigger_decisions_received;
  std::atomic<size_t> m_num_trb_completions_received;
  std::atomic<size_t> m_num_trigger_decision_tokens_received;
  std::atomic<size_t> m_num_status_requests_received;
  std::atomic<size_t> m_num_status_messages_sent;
  std::atomic<size_t> m_num_trigger_decisions_sent;

  std::atomic<size_t> m_num_duplicate_decisions_received;
  std::atomic<size_t> m_num_unexpected_trb_completions_received;
  std::atomic<size_t> m_num_unexpected_trigger_decision_tokens_received;
  std::atomic<size_t> m_num_early_trigger_decision_tokens_received;
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_DATAFLOWSTATUSMODULE_HPP_