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

#include "dfmodules/DataflowApplicationData.hpp"

#include "dfmessages/DataRequest.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "ipm/Receiver.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq {

ERS_DECLARE_ISSUE(dfmodules,
                  TriggerInjected,
                  "Injected " << count << " triggers in the system",
                  ((decltype(dfmodules::datafloworchestrator::ConfParams::initial_token_count))count))

namespace dfmodules {

/**
 * @brief DataFlowOrchestrator distributes triggers according to the availability of the DF apps in the system
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

  std::map<std::string, DataflowApplicationData> m_dataflow_availability;
  std::function<void(nlohmann::json&)> m_metadata_function;

private:
  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);
  void do_scrap(const data_t&);

  void do_work(std::atomic<bool>& run_flag);

  void get_info(opmonlib::InfoCollector& ci, int level) override;

  void receive_token(ipm::Receiver::Response message);
  bool has_slot() const;
  bool extract_a_decision(dfmessages::TriggerDecision& decision, std::atomic<bool>& run_flag);
  bool dispatch(dfmessages::TriggerDecision decision,
                std::shared_ptr<AssignedTriggerDecision> assignment_info,
                std::atomic<bool>& run_flag);

  // Configuration
  std::chrono::milliseconds m_queue_timeout;
  dunedaq::daqdataformats::run_number_t m_run_number;
  std::string m_token_connection_name;

  // Queue(s)
  using triggerdecisionsource_t = dunedaq::appfwk::DAQSource<dfmessages::TriggerDecision>;
  std::unique_ptr<triggerdecisionsource_t> m_trigger_decision_queue = nullptr;

  // Coordination
  appfwk::ThreadHelper m_working_thread;
  std::condition_variable m_slot_available_cv;
  mutable std::mutex m_slot_available_mutex;

  // Statistics
  std::atomic<uint64_t> m_received_tokens{ 0 };    // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_sent_decisions{ 0 };     // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_received_decisions{ 0 }; // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_dataflow_busy{ 0 };
  std::atomic<uint64_t> m_waiting_for_decision{ 0 };
  std::atomic<uint64_t> m_dfo_busy{ 0 };
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_
