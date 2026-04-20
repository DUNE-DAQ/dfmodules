/**
 * @file DFOConsensusModule.hpp
 *
 * DFOConsensusModule implements a consensus algorithm that allows multiple DFO
 * instances to run concurrently without assigning the same TriggerDecision
 * more than once, while each instance maintains full knowledge of the global
 * TRBModule slot state so it can issue accurate TriggerInhibit messages to the
 * MLT.
 *
 * Partition assignment
 * --------------------
 * Each DFO discovers its peers by exchanging announcement tokens at start-up.
 * Peers are sorted alphabetically; the DFO's index equals its position in that
 * sorted list.  A TriggerDecision is handled by the DFO whose index satisfies
 * trigger_number % num_dfos == own_index.
 *
 * DFODecision propagation
 * -----------------------
 * After the responsible DFO assigns a TriggerDecision to a TRBModule it
 * broadcasts a DFODecision message to all peer DFOs.  A DFODecision is also
 * sent when the TRBModule completes the trigger (token received).  Peers use
 * these messages to update their shadow view of each TRB's slot count.
 *
 * The inhibit signal is asserted when ALL TRBModules in the system are busy,
 * considering both own and peer assignments.
 *
 * Failover
 * --------
 * Every DFO buffers incoming TriggerDecisions with a reception timestamp.  If
 * the responsible DFO does not broadcast a DFODecision within
 * s_dfo_decision_timeout, surviving DFOs exclude it from the ensemble,
 * recompute the partition, and the DFO that now owns the trigger_number
 * re-assigns and broadcasts a DFODecision.
 *
 * A DFO with zero peer output connections operates as a standalone DFO,
 * identical to DFOModule.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_DFOCONSENSUSMODULE_HPP_
#define DFMODULES_PLUGINS_DFOCONSENSUSMODULE_HPP_

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
                  "DFOConsensusModule " << module_name << ": Timed out waiting for " << expected_peers
                    << " peer(s) to announce; received " << received_peers
                    << ". Continuing with the peers that responded.",
                  ((std::string)module_name)((size_t)expected_peers)((size_t)received_peers))

ERS_DECLARE_ISSUE(dfmodules,
                  DFOConsensusPartitionInfo,
                  "DFOConsensusModule " << module_name << ": Partition index " << own_index << " of " << num_dfos
                    << " DFO(s) in the ensemble.",
                  ((std::string)module_name)((size_t)own_index)((size_t)num_dfos))

ERS_DECLARE_ISSUE(dfmodules,
                  DFOConsensusFailover,
                  "DFOConsensusModule " << module_name << ": DFO peer " << failed_dfo
                    << " timed out for trigger_number " << trigger_number
                    << ". Removing from ensemble and reassigning.",
                  ((std::string)module_name)((std::string)failed_dfo)((uint32_t)trigger_number))
// Re-enable coverage checking LCOV_EXCL_STOP

namespace dfmodules {

/**
 * @brief DFOConsensusModule distributes triggers across multiple DFO instances
 *        with global TRBModule state visibility via DFODecision messages and
 *        automatic failover when a peer DFO stops responding.
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

  /**
   * @brief Timeout after which, if no DFODecision has been received for an
   *        accepted TriggerDecision, the responsible peer is considered failed
   *        and the partition is recomputed.
   */
  static constexpr std::chrono::milliseconds s_dfo_decision_timeout{ 2000 };

private:
  // Commands
  void do_conf(const CommandData_t&);
  void do_start(const CommandData_t&);
  void do_stop(const CommandData_t&);
  void do_scrap(const CommandData_t&);

  void generate_opmon_data() override;

  // --------------------------------------------------------------------------
  // Peer-announcement helpers
  // --------------------------------------------------------------------------

  /// Send this DFO's peer-announcement token to all configured peer outputs.
  void send_peer_announcement();

  /// (Re-)compute partition index and ensemble size from the current set of
  /// known peer names.  Must be called with m_peers_mutex NOT held.
  void compute_partition();

  // --------------------------------------------------------------------------
  // IOManager callbacks
  // --------------------------------------------------------------------------

  /// Token callback: intercepts peer-announcement tokens; passes everything
  /// else to DFOCore::receive_token().
  void on_token(const dfmessages::TriggerDecisionToken& token);

  /// TD callback: buffers the TD for failover; if responsible, delegates to
  /// DFOCore::receive_trigger_decision().
  void on_trigger_decision(const dfmessages::TriggerDecision& decision);

  /// DFODecision callback: updates shadow TRB slot counts and removes the
  /// corresponding entry from the pending-TD buffer.
  void on_dfo_decision(const DFODecision& msg);

  // --------------------------------------------------------------------------
  // DFODecision broadcasting
  // --------------------------------------------------------------------------

  /// Broadcast a DFODecision to all peer DFOs.
  void broadcast_dfo_decision(daqdataformats::trigger_number_t trigger_number,
                               const std::string& trb_conn,
                               size_t trb_slot_count,
                               bool is_completion);

  /// Callback registered with DFOCore: called after each successful assignment.
  void on_assignment(const std::shared_ptr<AssignedTriggerDecision>& atd, size_t trb_slot_count);

  /// Callback registered with DFOCore: called after each token completion.
  void on_completion(const std::string& trb_conn,
                     daqdataformats::trigger_number_t trigger_number,
                     size_t trb_slot_count);

  // --------------------------------------------------------------------------
  // Global busy check
  // --------------------------------------------------------------------------

  /// Returns true if ALL TRBModules in the system are busy, considering both
  /// own slot counts (from DFOCore) and remote slot counts from peer DFOs.
  bool is_globally_busy() const;

  // --------------------------------------------------------------------------
  // Watchdog / failover
  // --------------------------------------------------------------------------

  /// Watchdog thread body: periodically scans the pending-TD buffer and
  /// triggers failover when a responsible peer misses its DFODecision deadline.
  void watchdog_thread_func();

  /// Remove the DFO at ensemble index @p failed_index from m_registered_peers,
  /// recompute the partition, and reassign any pending TDs now owned by this DFO.
  /// Must be called with m_peers_mutex NOT held.
  void handle_peer_failure(size_t failed_index,
                           daqdataformats::trigger_number_t trigger_number);

  // --------------------------------------------------------------------------
  // Core DFO processing logic (common with DFOModule)
  // --------------------------------------------------------------------------
  std::unique_ptr<DFOCore> m_core;

  // --------------------------------------------------------------------------
  // Configuration
  // --------------------------------------------------------------------------
  const appmodel::DFOConf* m_dfo_conf{ nullptr };

  // --------------------------------------------------------------------------
  // Connections (initialised in init(), used at start())
  // --------------------------------------------------------------------------
  std::shared_ptr<iomanager::SenderConcept<dfmessages::TriggerInhibit>> m_busy_sender;
  std::string m_token_connection;
  std::string m_td_connection;
  std::vector<std::string> m_trb_conn_ids;

  /// Peer DFO output connections for sending peer-announcement tokens.
  std::vector<std::string> m_dfo_peer_output_connections;
  size_t m_expected_peers{ 0 };

  /// DFODecision output connections (one per peer DFO, for state propagation).
  std::vector<std::string> m_dfo_decision_output_connections;
  /// DFODecision input connection (receives decisions from all peer DFOs).
  std::string m_dfo_decision_input_connection;

  // --------------------------------------------------------------------------
  // Peer-announcement state (protected by m_peers_mutex)
  // --------------------------------------------------------------------------
  std::set<std::string> m_registered_peers;
  mutable std::mutex m_peers_mutex;
  std::condition_variable m_peers_cv;

  // --------------------------------------------------------------------------
  // Partition information (updated atomically)
  // --------------------------------------------------------------------------
  std::atomic<size_t> m_own_index{ 0 };
  std::atomic<size_t> m_num_dfos{ 1 };

  // --------------------------------------------------------------------------
  // Remote TRB slot shadow state
  // remote_slot_counts[source_dfo][trb_conn] = absolute slot count from that DFO
  // Protected by m_remote_slots_mutex.
  // --------------------------------------------------------------------------
  std::map<std::string, std::map<std::string, size_t>> m_remote_slot_counts;
  mutable std::mutex m_remote_slots_mutex;

  // --------------------------------------------------------------------------
  // Pending-TD buffer for failover
  // Holds every incoming TriggerDecision until a matching DFODecision is
  // received (or the DFO is stopped).
  // Protected by m_pending_tds_mutex.
  // --------------------------------------------------------------------------
  struct PendingTD
  {
    dfmessages::TriggerDecision decision;
    std::chrono::steady_clock::time_point received_at;
  };
  std::map<daqdataformats::trigger_number_t, PendingTD> m_pending_tds;
  mutable std::mutex m_pending_tds_mutex;

  // --------------------------------------------------------------------------
  // Watchdog thread
  // --------------------------------------------------------------------------
  std::thread m_watchdog_thread;
  std::atomic<bool> m_watchdog_running{ false };

  // --------------------------------------------------------------------------
  // Timing constants
  // --------------------------------------------------------------------------
  static constexpr std::chrono::milliseconds s_peer_announce_timeout{ 500 };
  static constexpr std::chrono::milliseconds s_watchdog_interval{ 100 };
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_DFOCONSENSUSMODULE_HPP_

