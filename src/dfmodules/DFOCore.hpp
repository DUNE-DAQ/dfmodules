/**
 * @file DFOCore.hpp  Core DFO processing logic shared by DFOModule and
 *                    DFOConsensusModule.
 *
 * DFOCore encapsulates TRB connection management, trigger-decision dispatching
 * and token processing that is common to both DFO module variants.  It is used
 * via composition (not inheritance) so that each module can inherit directly
 * from DAQModule while still sharing the same processing logic.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DFMODULES_DFOCORE_HPP_
#define DFMODULES_SRC_DFMODULES_DFOCORE_HPP_

#include "dfmodules/TriggerRecordBuilderData.hpp"

#include "daqdataformats/Types.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TriggerInhibit.hpp"
#include "iomanager/Sender.hpp"
#include "trgdataformats/TriggerCandidateData.hpp"

#include "logging/Logging.hpp" // NOTE: ERS issues must be declared after this header.

#include "nlohmann/json.hpp"

#include <atomic>
#include <bitset>
#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

namespace dunedaq {

// Disable coverage checking LCOV_EXCL_START
ERS_DECLARE_ISSUE(dfmodules,
                  TRBModuleAppUpdate,
                  "TRBModule app " << connection_name << ": " << message,
                  ((std::string)connection_name)((std::string)message))
ERS_DECLARE_ISSUE(dfmodules,
                  UnknownTokenSource,
                  "Token from unknown source: " << connection_name,
                  ((std::string)connection_name))
ERS_DECLARE_ISSUE(dfmodules,
                  DFOModuleRunNumberMismatch,
                  "DFOModule encountered run number mismatch: recvd ("
                    << received_run_number << ") != " << run_number << " from " << src_app
                    << " for trigger_number " << trig_num,
                  ((uint32_t)received_run_number)((uint32_t)run_number)((std::string)src_app)(
                    (uint32_t)trig_num)) // NOLINT(build/unsigned)
ERS_DECLARE_ISSUE(dfmodules,
                  IncompleteTriggerDecision,
                  "TriggerDecision " << trigger_number << " didn't complete within timeout in run " << run_number,
                  ((uint32_t)trigger_number)((uint32_t)run_number)) // NOLINT(build/unsigned)
ERS_DECLARE_ISSUE(dfmodules,
                  UnableToAssign,
                  "TriggerDecision " << trigger_number << " could not be assigned",
                  ((uint32_t)trigger_number)) // NOLINT(build/unsigned)
ERS_DECLARE_ISSUE(dfmodules,
                  AssignedToBusyApp,
                  "TriggerDecision " << trigger_number << " was assigned to DF app " << app
                                     << " that was busy with " << used_slots << " TDs",
                  ((uint32_t)trigger_number)((std::string)app)((size_t)used_slots)) // NOLINT(build/unsigned)
// Re-enable coverage checking LCOV_EXCL_STOP

namespace dfmodules {

/**
 * @brief DFOCore encapsulates the core DFO processing logic shared between
 *        DFOModule and DFOConsensusModule.  It is used via composition.
 *
 * Usage:
 *  1. Construct with the owning module's name (used in log messages).
 *  2. Call configure() once per conf command.
 *  3. Call start() once per start command, passing in the busy sender, a
 *     functor to look up TriggerDecision senders by connection name, and
 *     (optionally) a functor invoked whenever a new TRB app registers.
 *  4. Register receive_token() and receive_trigger_decision() as IOManager
 *     callbacks from the owning module.
 *  5. Call stop() on drain_dataflow; it waits for in-flight TDs, flushes, and
 *     returns any incomplete AssignedTriggerDecisions for error reporting.
 *  6. Call scrap() on scrap.
 */
class DFOCore
{
public:
  /// Functor to obtain a TriggerDecision sender for the given connection name.
  using td_sender_fn_t =
    std::function<std::shared_ptr<iomanager::SenderConcept<dfmessages::TriggerDecision>>(const std::string&)>;

  /// Called whenever a new TRB app registers (run_number==0, trigger_number==0 token).
  /// Provides the connection name and the newly created TriggerRecordBuilderData so
  /// the owning DAQModule can register it as a child opmon node via register_node().
  using new_trb_fn_t =
    std::function<void(const std::string&, std::shared_ptr<TriggerRecordBuilderData>)>;

  /// Called after each successful trigger-decision assignment.
  /// Arguments: the AssignedTriggerDecision (with connection_name and decision),
  ///            and the current slot count for that TRB after the assignment.
  using on_assignment_fn_t =
    std::function<void(const std::shared_ptr<AssignedTriggerDecision>&, size_t trb_slot_count)>;

  /// Called after each trigger-decision token is processed (i.e., a TRB completed
  /// a trigger).  Arguments: TRB connection name, trigger_number, and the current
  /// slot count for that TRB after the completion.
  using on_completion_fn_t =
    std::function<void(const std::string&, daqdataformats::trigger_number_t, size_t trb_slot_count)>;

  /// Optional override for the busy check used by notify_trigger_if_needed().
  /// When set, this function is called instead of the default is_busy() to
  /// determine whether a TriggerInhibit should be sent.
  using is_busy_fn_t = std::function<bool()>;

  explicit DFOCore(std::string owner_name);

  DFOCore(const DFOCore&) = delete;
  DFOCore& operator=(const DFOCore&) = delete;
  DFOCore(DFOCore&&) = delete;
  DFOCore& operator=(DFOCore&&) = delete;

  // --------------------------------------------------------------------------
  // Lifecycle
  // --------------------------------------------------------------------------

  void configure(size_t busy_threshold,
                 size_t free_threshold,
                 size_t td_send_retries,
                 std::chrono::milliseconds queue_timeout,
                 std::chrono::microseconds stop_timeout);

  void start(daqdataformats::run_number_t run_number,
             std::shared_ptr<iomanager::SenderConcept<dfmessages::TriggerInhibit>> busy_sender,
             td_sender_fn_t get_td_sender_fn,
             new_trb_fn_t on_new_trb_fn = nullptr,
             on_assignment_fn_t on_assignment_fn = nullptr,
             on_completion_fn_t on_completion_fn = nullptr,
             is_busy_fn_t is_busy_fn = nullptr);

  /// Waits up to the configured stop_timeout for outstanding
  /// TDs then flushes and returns any incomplete AssignedTriggerDecisions.
  std::list<std::shared_ptr<AssignedTriggerDecision>> flush();

  void stop();

  void scrap();

  // --------------------------------------------------------------------------
  // Callbacks – to be registered with the IOManager by the owning module.
  // --------------------------------------------------------------------------

  void receive_token(const dfmessages::TriggerDecisionToken& token);
  void receive_trigger_decision(const dfmessages::TriggerDecision& decision);

  // --------------------------------------------------------------------------
  // State queries
  // --------------------------------------------------------------------------

  bool is_busy() const;
  bool is_empty() const;
  size_t used_slots() const;
  void notify_trigger_if_needed();

  /// Returns true if ALL TRBModules are busy when combining own slot counts
  /// with the @p extra_slots_per_trb map (e.g., remote peer slot counts).
  /// Used by DFOConsensusModule to determine global inhibit state.
  bool is_globally_busy(const std::map<std::string, size_t>& extra_slots_per_trb) const;

  daqdataformats::run_number_t run_number() const { return m_run_number; }
  std::chrono::milliseconds queue_timeout() const { return m_queue_timeout; }
  size_t num_trb_apps() const { return m_dataflow_availability.size(); }

  // --------------------------------------------------------------------------
  // Opmon helpers
  // --------------------------------------------------------------------------

  struct OpMonSnapshot
  {
    uint64_t tokens_received{ 0 };      // NOLINT(build/unsigned)
    uint64_t decisions_received{ 0 };   // NOLINT(build/unsigned)
    uint64_t decisions_sent{ 0 };       // NOLINT(build/unsigned)
    uint64_t waiting_for_decision{ 0 }; // NOLINT(build/unsigned)
    uint64_t deciding_destination{ 0 }; // NOLINT(build/unsigned)
    uint64_t forwarding_decision{ 0 };  // NOLINT(build/unsigned)
    uint64_t waiting_for_token{ 0 };    // NOLINT(build/unsigned)
    uint64_t processing_token{ 0 };     // NOLINT(build/unsigned)
  };

  /// Atomically exchange all counters to zero and return their previous values.
  OpMonSnapshot take_opmon_snapshot();

  struct TriggerData
  {
    std::atomic<uint64_t> received{ 0 };  // NOLINT(build/unsigned)
    std::atomic<uint64_t> completed{ 0 }; // NOLINT(build/unsigned)
  };

  std::map<trgdataformats::TriggerCandidateData::Type, TriggerData>& get_trigger_counters()
  {
    return m_trigger_counters;
  }
  std::mutex& get_trigger_counters_mutex() { return m_trigger_counters_mutex; }

  std::function<void(nlohmann::json&)>& metadata_function() { return m_metadata_function; }

private:
  std::string m_owner_name;

  // Runtime state
  daqdataformats::run_number_t m_run_number{ 0 };
  std::atomic<bool> m_running_status{ false };
  std::chrono::milliseconds m_queue_timeout{ 100 };
  std::chrono::microseconds m_stop_timeout{ 0 };

  // Configuration
  size_t m_busy_threshold{ 0 };
  size_t m_free_threshold{ 0 };
  size_t m_td_send_retries{ 0 };

  // Connections (set at start, cleared at scrap)
  std::shared_ptr<iomanager::SenderConcept<dfmessages::TriggerInhibit>> m_busy_sender;
  td_sender_fn_t m_get_td_sender_fn;
  new_trb_fn_t m_on_new_trb_fn;
  on_assignment_fn_t m_on_assignment_fn;
  on_completion_fn_t m_on_completion_fn;
  is_busy_fn_t m_is_busy_fn;

  std::function<void(nlohmann::json&)> m_metadata_function;

  // TRB management
  std::map<std::string, std::shared_ptr<TriggerRecordBuilderData>> m_dataflow_availability;
  std::map<std::string, std::shared_ptr<TriggerRecordBuilderData>>::iterator m_last_assignment_it;

  // Busy notification
  mutable std::atomic<bool> m_last_notified_busy{ false };
  mutable std::mutex m_notify_trigger_mutex;

  // Timing
  std::chrono::steady_clock::time_point m_last_token_received;
  std::chrono::steady_clock::time_point m_last_td_received;

  // Statistics
  std::atomic<uint64_t> m_received_tokens{ 0 };      // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_sent_decisions{ 0 };        // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_received_decisions{ 0 };    // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_waiting_for_decision{ 0 };  // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_deciding_destination{ 0 };  // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_forwarding_decision{ 0 };   // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_waiting_for_token{ 0 };     // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_processing_token{ 0 };      // NOLINT(build/unsigned)

  std::map<trgdataformats::TriggerCandidateData::Type, TriggerData> m_trigger_counters;
  std::mutex m_trigger_counters_mutex;

  // Private helpers
  std::shared_ptr<AssignedTriggerDecision> find_slot(const dfmessages::TriggerDecision& decision);
  bool dispatch(const std::shared_ptr<AssignedTriggerDecision>& assignment);
  void assign_trigger_decision(const std::shared_ptr<AssignedTriggerDecision>& assignment);

  TriggerData& get_trigger_counter(trgdataformats::TriggerCandidateData::Type type);

  static std::set<trgdataformats::TriggerCandidateData::Type>
  unpack_types(decltype(dfmessages::TriggerDecision::trigger_type) t)
  {
    std::set<trgdataformats::TriggerCandidateData::Type> results;
    if (t == dfmessages::TypeDefaults::s_invalid_trigger_type)
      return results;
    const std::bitset<64> bits(t);
    for (size_t i = 0; i < bits.size(); ++i) {
      if (bits[i])
        results.insert(static_cast<trgdataformats::TriggerCandidateData::Type>(i));
    }
    return results;
  }
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_DFMODULES_DFOCORE_HPP_
