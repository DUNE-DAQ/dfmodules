/**
 * @file DataFlowOrchestrator.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_
#define DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_

#include "dfmodules/datafloworchestrator/Structs.hpp"

#include "dfmodules/TriggerRecordBuilderData.hpp"

#include "dfmessages/DataRequest.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "ipm/Receiver.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSource.hpp"
#include "logging/Logging.hpp"
#include "utilities/WorkerThread.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq {

// Disable coverage checking LCOV_EXCL_START
ERS_DECLARE_ISSUE(dfmodules,
                  TriggerRecordBuilderAppUpdate,
                  "TriggerRecordBuilder app " << connection_name << ": " << message,
                  ((std::string)connection_name)((std::string)message))
ERS_DECLARE_ISSUE(dfmodules,
                  DataFlowOrchestratorRunNumberMismatch,
                  "DataFlowOrchestrator encountered run number mismatch: recvd ("
                    << received_run_number << ") != " << run_number << " from " << src_app,
                  ((uint32_t)received_run_number)((uint32_t)run_number)((std::string)src_app)) // NOLINT(build/unsigned)
// Re-enable coverage checking LCOV_EXCL_STOP

namespace dfmodules {

/**
 * @brief DataFlowOrchestrator distributes triggers according to the
 * availability of the DF apps in the system
 */
class DataFlowOrchestrator : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief DataFlowOrchestrator Constructor
   * @param name Instance name for this DataFlowOrchestrator instance
   */
  explicit DataFlowOrchestrator(const std::string& name);

  DataFlowOrchestrator(const DataFlowOrchestrator&) = delete; ///< DataFlowOrchestrator is not copy-constructible
  DataFlowOrchestrator& operator=(const DataFlowOrchestrator&) =
    delete;                                                         ///< DataFlowOrchestrator is not copy-assignable
  DataFlowOrchestrator(DataFlowOrchestrator&&) = delete;            ///< DataFlowOrchestrator is not move-constructible
  DataFlowOrchestrator& operator=(DataFlowOrchestrator&&) = delete; ///< DataFlowOrchestrator is not move-assignable

  void init(const data_t&) override;

protected:
  virtual std::shared_ptr<AssignedTriggerDecision> find_slot(dfmessages::TriggerDecision decision);

  std::map<std::string, TriggerRecordBuilderData> m_dataflow_availability;
  // std::map<std::string, TriggerRecordBuilderData>::iterator m_last_assignement_it;
  std::function<void(nlohmann::json&)> m_metadata_function;

private:
  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);
  void do_scrap(const data_t&);

  void get_info(opmonlib::InfoCollector& ci, int level) override;

  virtual void receive_trigger_complete_token(ipm::Receiver::Response message);
  void receive_trigger_decision(ipm::Receiver::Response message);
  virtual bool is_busy() const;
  void notify_trigger(bool busy) const;
  bool dispatch(std::shared_ptr<AssignedTriggerDecision> assignment);
  virtual void assign_trigger_decision(std::shared_ptr<AssignedTriggerDecision> assignment);

  // Configuration
  std::chrono::milliseconds m_queue_timeout;
  dunedaq::daqdataformats::run_number_t m_run_number;
  std::string m_token_connection_name;
  std::string m_busy_connection_name;
  std::string m_td_connection_name;
  size_t m_td_send_retries;

  // Coordination
  // utilities::WorkerThread m_working_thread;
  // std::condition_variable m_slot_available_cv;
  // mutable std::mutex m_slot_available_mutex;
  // atomic<bool> m_last_notifiled_status{false};
  std::atomic<bool> m_running_status{ false };
  mutable std::atomic<bool> m_last_notified_busy{ false };
  std::string m_last_sent_td_connection;
  std::chrono::steady_clock::time_point m_last_token_received;
  std::chrono::steady_clock::time_point m_last_td_received;

  // Statistics
  std::atomic<uint64_t> m_received_tokens{ 0 };      // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_sent_decisions{ 0 };       // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_received_decisions{ 0 };   // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_waiting_for_decision{ 0 }; // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_deciding_destination{ 0 }; // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_forwarding_decision{ 0 };  // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_waiting_for_token{ 0 };    // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_processing_token{ 0 };     // NOLINT (build/unsigned)
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_
