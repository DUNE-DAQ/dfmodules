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

#include "dfmessages/DataRequest.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "ipm/Receiver.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSource.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq {

  ERS_DECLARE_ISSUE(dfmodules, TriggerInjected, "Injected " << count << " triggers in the system", 
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

private:
  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);
  void do_scrap(const data_t&);

  void get_info(opmonlib::InfoCollector& ci, int level) override;

  void send_initial_triggers();

  bool extract_a_decision(dfmessages::TriggerDecision&);

  void receive_tokens(ipm::Receiver::Response message);

  bool dispatch(dfmessages::TriggerDecision&&);

  // Configuration
  std::chrono::milliseconds m_queue_timeout;
  dunedaq::daqdataformats::run_number_t m_run_number;
  decltype(datafloworchestrator::ConfParams::initial_token_count) m_initial_tokens;
  std::string m_td_connection_name = "";
  std::string m_token_connection_name = "";

  // Queue(s)
  using triggerdecisionsource_t = dunedaq::appfwk::DAQSource<dfmessages::TriggerDecision>;
  std::unique_ptr<triggerdecisionsource_t> m_trigger_decision_queue = nullptr;

  std::atomic<bool> m_is_running{ false };
  std::atomic<uint64_t> m_received_tokens{ 0 }; // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_sent_decisions{ 0 };   // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_received_decisions{ 0 };   // NOLINT (build/unsigned)
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_REQUESTRECEIVER_HPP_
