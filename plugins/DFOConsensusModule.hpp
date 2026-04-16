/**
 * @file DFOConsensusModule.hpp
 *
 * DFOConsensusModule implements a consensus algorithm that allows multiple DFO
 * instances to run concurrently without assigning the same TriggerDecision
 * more than once.
 *
 * The consensus is achieved through deterministic partitioning:
 *   - Each DFO instance discovers its peers by exchanging announcement tokens
 *     at start-up via its token input connection.
 *   - Peers are sorted alphabetically by module name.  Each DFO's partition
 *     index equals its position in that sorted list.
 *   - A TriggerDecision is handled by exactly the DFO whose index satisfies
 *     trigger_number % num_dfos == own_index.
 *
 * This approach requires no round-trip coordination per decision and therefore
 * adds negligible latency.  A DFO with zero configured peer output connections
 * operates as a standalone DFO, identical to DFOModule.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_DFOCONSENSUSMODULE_HPP_
#define DFMODULES_PLUGINS_DFOCONSENSUSMODULE_HPP_

#include "dfmodules/DFOCore.hpp"

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
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace dunedaq {

// Disable coverage checking LCOV_EXCL_START
ERS_DECLARE_ISSUE(dfmodules,
                  DFOConsensusPeerTimeout,
                  "DFOConsensusModule " << module_name << ": Timed out waiting for " << expected_peers
                    << " peer(s) to announce; received " << received_peers
                    << ". Continuing with the peers that responded.",
                  ((std::string)module_name)((size_t)expected_peers)((size_t)received_peers))

ERS_DECLARE_ISSUE(dfmodules,
                  DFOConsensusPartitionInfo,
                  "DFOConsensusModule " << module_name << ": Partition index " << own_index << " of " << num_dfos
                    << " DFO(s) in the ensemble.",
                  ((std::string)module_name)((size_t)own_index)((size_t)num_dfos))
// Re-enable coverage checking LCOV_EXCL_STOP

namespace dfmodules {

/**
 * @brief DFOConsensusModule distributes triggers across multiple DFO instances
 *        without double-assignment, using deterministic trigger-number
 *        partitioning and peer-announcement via TriggerDecisionToken messages.
 *
 *        Inherits directly from DAQModule; common DFO processing logic is
 *        handled by DFOCore via composition.
 */
class DFOConsensusModule : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief DFOConsensusModule Constructor
   * @param name Instance name for this DFOConsensusModule instance
   */
  explicit DFOConsensusModule(const std::string& name);

  DFOConsensusModule(const DFOConsensusModule&) = delete;            ///< Not copy-constructible
  DFOConsensusModule& operator=(const DFOConsensusModule&) = delete; ///< Not copy-assignable
  DFOConsensusModule(DFOConsensusModule&&) = delete;                 ///< Not move-constructible
  DFOConsensusModule& operator=(DFOConsensusModule&&) = delete;      ///< Not move-assignable

  void init(std::shared_ptr<appfwk::ConfigurationManager> mcfg) override;

  /**
   * @brief Magic trigger_number value in a TriggerDecisionToken that identifies
   *        a DFO peer-announcement message rather than a normal completion token.
   */
  static constexpr daqdataformats::trigger_number_t s_peer_announce_magic =
    std::numeric_limits<daqdataformats::trigger_number_t>::max();

private:
  // Commands
  void do_conf(const CommandData_t&);
  void do_start(const CommandData_t&);
  void do_stop(const CommandData_t&);
  void do_scrap(const CommandData_t&);

  void generate_opmon_data() override;

  /// Send this DFO's peer-announcement token to all configured peer outputs.
  void send_peer_announcement();

  /**
   * @brief (Re-)compute partition index and ensemble size from the current set
   *        of known peer names.  Must be called with m_peers_mutex NOT held.
   */
  void compute_partition();

  /// Token callback: intercepts peer-announcement tokens; passes everything
  /// else to DFOCore::receive_token().
  void on_token(const dfmessages::TriggerDecisionToken& token);

  /// TD callback: applies the partition filter and, if accepted, delegates to
  /// DFOCore::receive_trigger_decision().
  void on_trigger_decision(const dfmessages::TriggerDecision& decision);

  // Core DFO processing logic (common with DFOModule)
  std::unique_ptr<DFOCore> m_core;

  // Configuration
  const appmodel::DFOConf* m_dfo_conf{ nullptr };

  // Connections (initialised in init(), used at start())
  std::shared_ptr<iomanager::SenderConcept<dfmessages::TriggerInhibit>> m_busy_sender;
  std::string m_token_connection;
  std::string m_td_connection;
  std::vector<std::string> m_trb_conn_ids;

  // Peer DFO output connections (one per peer DFO, for sending announcements)
  std::vector<std::string> m_dfo_peer_output_connections;
  size_t m_expected_peers{ 0 };

  // Peer-announcement state (protected by m_peers_mutex)
  std::set<std::string> m_registered_peers;
  mutable std::mutex m_peers_mutex;
  std::condition_variable m_peers_cv;

  // Partition information (updated atomically)
  std::atomic<size_t> m_own_index{ 0 };
  std::atomic<size_t> m_num_dfos{ 1 };

  // How long to wait at start for peer announcements before proceeding
  static constexpr std::chrono::milliseconds s_peer_announce_timeout{ 500 };
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_DFOCONSENSUSMODULE_HPP_
