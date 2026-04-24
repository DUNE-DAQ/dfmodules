/**
 * @file DFOModule.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_
#define DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_

#include "dfmodules/DFOCore.hpp"
#include "dfmodules/DFODecision.hpp"

#include "appmodel/DFOConf.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TriggerInhibit.hpp"
#include "iomanager/Sender.hpp"

#include "appfwk/DAQModule.hpp"
#include "logging/Logging.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace dunedaq {

// Disable coverage checking LCOV_EXCL_START
ERS_DECLARE_ISSUE(dfmodules,
                  DFOConsensusPeerTimeout,
                  "DFOModule " << module_name << ": Timed out waiting for " << expected_peers
                    << " peer(s) to announce; received " << received_peers
                    << ". Continuing with the peers that responded.",
                  ((std::string)module_name)((size_t)expected_peers)((size_t)received_peers))

ERS_DECLARE_ISSUE(dfmodules,
                  DFOConsensusPartitionInfo,
                  "DFOModule " << module_name << ": Partition index " << own_index << " of " << num_dfos
                    << " DFO(s) in the ensemble.",
                  ((std::string)module_name)((size_t)own_index)((size_t)num_dfos))

ERS_DECLARE_ISSUE(dfmodules,
                  DFOConsensusFailover,
                  "DFOModule " << module_name << ": DFO peer " << failed_dfo
                    << " timed out for trigger_number " << trigger_number
                    << ". Removing from ensemble and reassigning.",
                  ((std::string)module_name)((std::string)failed_dfo)((uint32_t)trigger_number))
// Re-enable coverage checking LCOV_EXCL_STOP

namespace dfmodules {

/**
 * @brief DFOModule distributes triggers according to the availability of TRB apps.
 *
 * If consensus mode is enabled in DFOConf (or forced by the
 * DFOConsensusModule compatibility wrapper), DFOModule also coordinates with
 * peer DFOs, partitions TriggerDecision processing, propagates DFODecision
 * state and performs timeout-based failover.
 */
class DFOModule : public dunedaq::appfwk::DAQModule
{
public:
  explicit DFOModule(const std::string& name);

  DFOModule(const DFOModule&) = delete;
  DFOModule& operator=(const DFOModule&) = delete;
  DFOModule(DFOModule&&) = delete;
  DFOModule& operator=(DFOModule&&) = delete;

  void init(std::shared_ptr<appfwk::ConfigurationManager> mcfg) override;

  static constexpr daqdataformats::trigger_number_t s_peer_announce_magic =
    std::numeric_limits<daqdataformats::trigger_number_t>::max();

private:
  void do_conf(const CommandData_t&);
  void do_start(const CommandData_t&);
  void do_stop(const CommandData_t&);
  void do_scrap(const CommandData_t&);

  void generate_opmon_data() override;

  void send_peer_announcement();
  void compute_partition();

  void on_token(const dfmessages::TriggerDecisionToken& token);
  void on_trigger_decision(const dfmessages::TriggerDecision& decision);
  void on_dfo_decision(const DFODecision& msg);

  void broadcast_dfo_decision(daqdataformats::trigger_number_t trigger_number,
                              const std::string& trb_conn,
                              size_t trb_slot_count,
                              bool is_completion);
  void on_assignment(const std::shared_ptr<AssignedTriggerDecision>& atd, size_t trb_slot_count);
  void on_completion(const std::string& trb_conn,
                     daqdataformats::trigger_number_t trigger_number,
                     size_t trb_slot_count);

  bool is_globally_busy() const;

  void watchdog_thread_func();
  void handle_peer_failure(size_t failed_index,
                           daqdataformats::trigger_number_t trigger_number);

  std::unique_ptr<DFOCore> m_core;

  const appmodel::DFOConf* m_dfo_conf{ nullptr };

  std::shared_ptr<iomanager::SenderConcept<dfmessages::TriggerInhibit>> m_busy_sender;
  std::string m_token_connection;
  std::string m_td_connection;
  std::vector<std::string> m_trb_conn_ids;

  bool m_consensus_enabled{ false };
  std::vector<std::string> m_dfo_peer_output_connections;
  size_t m_expected_peers{ 0 };
  std::vector<std::string> m_dfo_decision_output_connections;
  std::string m_dfo_decision_input_connection;
  std::chrono::milliseconds m_dfo_decision_timeout{ 2000 };
  std::chrono::milliseconds m_peer_announce_timeout{ 500 };
  std::chrono::milliseconds m_watchdog_interval{ 100 };
  std::set<std::string> m_registered_peers;
  mutable std::mutex m_peers_mutex;
  std::condition_variable m_peers_cv;
  std::atomic<size_t> m_own_index{ 0 };
  std::atomic<size_t> m_num_dfos{ 1 };
  std::map<std::string, std::map<std::string, size_t>> m_remote_slot_counts;
  mutable std::mutex m_remote_slots_mutex;

  struct PendingTD
  {
    dfmessages::TriggerDecision decision;
    std::chrono::steady_clock::time_point received_at;
  };
  std::map<daqdataformats::trigger_number_t, PendingTD> m_pending_tds;
  mutable std::mutex m_pending_tds_mutex;

  std::thread m_watchdog_thread;
  std::atomic<bool> m_watchdog_running{ false };

};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_
