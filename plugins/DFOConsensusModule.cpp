/**
 * @file DFOConsensusModule.cpp DFOConsensusModule class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DFOConsensusModule.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "dfmodules/opmon/DFOModule.pb.h"

#include "appmodel/DFOModule.hpp"
#include "confmodel/Connection.hpp"
#include "iomanager/IOManager.hpp"
#include "logging/Logging.hpp"

#include "trgdataformats/TriggerCandidateData.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "DFOConsensusModule" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_PEER_ANNOUNCE = 6,
  TLVL_PARTITION = 7,
  TLVL_TD_FILTER = 10,
  TLVL_DFO_DECISION = 11,
  TLVL_WATCHDOG = 12
};

namespace dunedaq::dfmodules {

DFOConsensusModule::DFOConsensusModule(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_core(std::make_unique<DFOCore>(name))
{
  register_command("conf", &DFOConsensusModule::do_conf);
  register_command("start", &DFOConsensusModule::do_start);
  register_command("drain_dataflow", &DFOConsensusModule::do_stop);
  register_command("scrap", &DFOConsensusModule::do_scrap);
}

void
DFOConsensusModule::init(std::shared_ptr<appfwk::ConfigurationManager> mcfg)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  auto mdal = mcfg->get_dal<appmodel::DFOModule>(get_name());
  if (!mdal) {
    throw appfwk::CommandFailed(ERS_HERE, "init", get_name(), "Unable to retrieve configuration object");
  }
  auto iom = iomanager::IOManager::get();

  for (auto con : mdal->get_inputs()) {
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerDecisionToken>()) {
      m_token_connection = con->UID();
    }
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerDecision>()) {
      m_td_connection = con->UID();
    }
    if (con->get_data_type() == datatype_to_string<DFODecision>()) {
      m_dfo_decision_input_connection = con->UID();
      TLOG_DEBUG(TLVL_DFO_DECISION) << get_name() << ": Found DFODecision input connection: " << con->UID();
    }
  }
  for (auto con : mdal->get_outputs()) {
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerInhibit>()) {
      m_busy_sender = iom->get_sender<dfmessages::TriggerInhibit>(con->UID());
    }
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerDecision>()) {
      m_trb_conn_ids.push_back(con->UID());
    }
    // Peer DFO output connections carry TriggerDecisionToken messages.
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerDecisionToken>()) {
      m_dfo_peer_output_connections.push_back(con->UID());
      TLOG_DEBUG(TLVL_PEER_ANNOUNCE) << get_name() << ": Found peer DFO output connection: " << con->UID();
    }
    if (con->get_data_type() == datatype_to_string<DFODecision>()) {
      m_dfo_decision_output_connections.push_back(con->UID());
      TLOG_DEBUG(TLVL_DFO_DECISION) << get_name() << ": Found DFODecision output connection: " << con->UID();
    }
  }

  if (m_token_connection.empty()) {
    throw appfwk::MissingConnection(
      ERS_HERE, get_name(), datatype_to_string<dfmessages::TriggerDecisionToken>(), "input");
  }
  if (m_td_connection.empty()) {
    throw appfwk::MissingConnection(
      ERS_HERE, get_name(), datatype_to_string<dfmessages::TriggerDecision>(), "input");
  }
  if (m_busy_sender == nullptr) {
    throw appfwk::MissingConnection(
      ERS_HERE, get_name(), datatype_to_string<dfmessages::TriggerInhibit>(), "output");
  }

  m_dfo_conf = mdal->get_configuration();
  m_expected_peers = m_dfo_peer_output_connections.size();

  // Verify that receivers exist (fetches connection details eagerly)
  iom->get_receiver<dfmessages::TriggerDecisionToken>(m_token_connection);
  iom->get_receiver<dfmessages::TriggerDecision>(m_td_connection);

  TLOG() << get_name() << ": DFOConsensusModule initialized with " << m_expected_peers << " expected peer DFO(s)"
         << " and " << m_dfo_decision_output_connections.size() << " DFODecision output connection(s)";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DFOConsensusModule::do_conf(const CommandData_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  m_core->configure(m_dfo_conf->get_busy_threshold(),
                    m_dfo_conf->get_free_threshold(),
                    m_dfo_conf->get_td_send_retries(),
                    std::chrono::milliseconds(m_dfo_conf->get_general_queue_timeout_ms()),
                    std::chrono::milliseconds(m_dfo_conf->get_stop_timeout_ms()));

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method, there are "
                                      << m_core->num_trb_apps() << " TRB apps defined";
}

void
DFOConsensusModule::do_start(const CommandData_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  // Reset peer state from any previous run.
  {
    std::lock_guard<std::mutex> guard(m_peers_mutex);
    m_registered_peers.clear();
  }
  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    m_remote_slot_counts.clear();
  }
  {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds.clear();
  }

  auto run_number = payload.value<daqdataformats::run_number_t>("run", 0);

  auto iom = iomanager::IOManager::get();
  if (m_busy_sender != nullptr) {
    bool is_ready = m_busy_sender->is_ready_for_sending(std::chrono::milliseconds(100));
    TLOG_DEBUG(0) << "The sender for TriggerInhibit messages " << (is_ready ? "is" : "is not") << " ready.";
  }
  for (auto& trb_conn : m_trb_conn_ids) {
    auto sender = iom->get_sender<dfmessages::TriggerDecision>(trb_conn);
    if (sender != nullptr) {
      bool is_ready = sender->is_ready_for_sending(std::chrono::milliseconds(100));
      TLOG_DEBUG(0) << "The TriggerDecision sender for " << trb_conn << " "
                    << (is_ready ? "is" : "is not") << " ready.";
    }
  }

  m_core->start(run_number,
                m_busy_sender,
                [iom](const std::string& conn) {
                  return iom->get_sender<dfmessages::TriggerDecision>(conn);
                },
                [this](const std::string& name, std::shared_ptr<TriggerRecordBuilderData> trbd) {
                  register_node(name, trbd);
                },
                [this](const std::shared_ptr<AssignedTriggerDecision>& atd, size_t slot_count) {
                  on_assignment(atd, slot_count);
                },
                [this](const std::string& trb_conn,
                       daqdataformats::trigger_number_t tn,
                       size_t slot_count) { on_completion(trb_conn, tn, slot_count); },
                [this]() { return is_globally_busy(); });

  iom->add_callback<dfmessages::TriggerDecisionToken>(
    m_token_connection, std::bind(&DFOConsensusModule::on_token, this, std::placeholders::_1));

  iom->add_callback<dfmessages::TriggerDecision>(
    m_td_connection, std::bind(&DFOConsensusModule::on_trigger_decision, this, std::placeholders::_1));

  if (!m_dfo_decision_input_connection.empty()) {
    iom->add_callback<DFODecision>(
      m_dfo_decision_input_connection,
      std::bind(&DFOConsensusModule::on_dfo_decision, this, std::placeholders::_1));
  }

  // Broadcast our identity to all peer DFOs.
  send_peer_announcement();

  // Wait until all expected peers have responded (or the timeout expires).
  if (m_expected_peers > 0) {
    std::unique_lock<std::mutex> lock(m_peers_mutex);
    bool all_peers_ready = m_peers_cv.wait_for(lock, s_peer_announce_timeout, [this] {
      return m_registered_peers.size() >= m_expected_peers;
    });
    if (!all_peers_ready) {
      ers::warning(DFOConsensusPeerTimeout(
        ERS_HERE, get_name(), m_expected_peers, m_registered_peers.size()));
    }
  }

  // Determine our partition index from the complete peer set.
  compute_partition();
  ers::info(DFOConsensusPartitionInfo(ERS_HERE, get_name(), m_own_index.load(), m_num_dfos.load()));

  // Start the watchdog thread if there are peer DFOs in the ensemble.
  if (m_num_dfos.load() > 1 || !m_dfo_decision_output_connections.empty()) {
    m_watchdog_running.store(true);
    m_watchdog_thread = std::thread(&DFOConsensusModule::watchdog_thread_func, this);
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DFOConsensusModule::do_stop(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  // Stop the watchdog thread before halting DFOCore so the watchdog does not
  // try to dispatch TDs while DFOCore is draining.
  m_watchdog_running.store(false);
  if (m_watchdog_thread.joinable()) {
    m_watchdog_thread.join();
  }

  m_core->stop();

  auto iom = iomanager::IOManager::get();
  iom->remove_callback<dfmessages::TriggerDecision>(m_td_connection);

  auto remnants = m_core->flush();

  iom->remove_callback<dfmessages::TriggerDecisionToken>(m_token_connection);
  if (!m_dfo_decision_input_connection.empty()) {
    iom->remove_callback<DFODecision>(m_dfo_decision_input_connection);
  }

  for (auto& r : remnants) {
    ers::error(IncompleteTriggerDecision(ERS_HERE, r->decision.trigger_number, m_core->run_number()));
  }

  // Clear runtime state.
  {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds.clear();
  }
  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    m_remote_slot_counts.clear();
  }

  // Reset to standalone mode so a subsequent start is clean.
  m_own_index.store(0);
  m_num_dfos.store(1);

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DFOConsensusModule::do_scrap(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  m_core->scrap();

  TLOG() << get_name() << " successfully scrapped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
DFOConsensusModule::generate_opmon_data()
{
  auto snap = m_core->take_opmon_snapshot();

  opmon::DFOInfo info;
  info.set_tokens_received(snap.tokens_received);
  info.set_decisions_sent(snap.decisions_sent);
  info.set_decisions_received(snap.decisions_received);
  info.set_waiting_for_decision(snap.waiting_for_decision);
  info.set_deciding_destination(snap.deciding_destination);
  info.set_forwarding_decision(snap.forwarding_decision);
  info.set_waiting_for_token(snap.waiting_for_token);
  info.set_processing_token(snap.processing_token);
  publish(std::move(info));

  std::lock_guard<std::mutex> guard(m_core->get_trigger_counters_mutex());
  for (auto& [type, counts] : m_core->get_trigger_counters()) {
    opmon::TriggerInfo ti;
    ti.set_received(counts.received.exchange(0));
    ti.set_completed(counts.completed.exchange(0));
    auto name = dunedaq::trgdataformats::get_trigger_candidate_type_names()[type];
    publish(std::move(ti), { { "type", name } });
  }
}

// ---------------------------------------------------------------------------
// Peer-announcement helpers
// ---------------------------------------------------------------------------

void
DFOConsensusModule::send_peer_announcement()
{
  if (m_dfo_peer_output_connections.empty())
    return;

  dfmessages::TriggerDecisionToken announcement;
  announcement.run_number = 0;
  announcement.trigger_number = s_peer_announce_magic;
  announcement.decision_destination = get_name();

  auto iom = iomanager::IOManager::get();
  for (const auto& conn : m_dfo_peer_output_connections) {
    try {
      auto announcement_copy = announcement;
      iom->get_sender<dfmessages::TriggerDecisionToken>(conn)->send(
        std::move(announcement_copy), m_core->queue_timeout());
      TLOG_DEBUG(TLVL_PEER_ANNOUNCE) << get_name() << ": Sent peer announcement to " << conn;
    } catch (const ers::Issue& excpt) {
      ers::warning(excpt);
    }
  }
}

void
DFOConsensusModule::compute_partition()
{
  std::vector<std::string> ensemble;
  {
    std::lock_guard<std::mutex> guard(m_peers_mutex);
    ensemble.push_back(get_name());
    for (const auto& peer : m_registered_peers) {
      ensemble.push_back(peer);
    }
  }

  // Sort names alphabetically to obtain a deterministic, agreed-upon order.
  std::sort(ensemble.begin(), ensemble.end());

  auto it = std::find(ensemble.begin(), ensemble.end(), get_name());
  size_t own_index = (it != ensemble.end()) ? static_cast<size_t>(std::distance(ensemble.begin(), it)) : 0;

  m_own_index.store(own_index);
  m_num_dfos.store(ensemble.size());

  TLOG_DEBUG(TLVL_PARTITION) << get_name() << ": Partition computed: index=" << own_index << " of "
                              << ensemble.size() << " DFO(s)";
}

// ---------------------------------------------------------------------------
// IOManager callbacks
// ---------------------------------------------------------------------------

void
DFOConsensusModule::on_token(const dfmessages::TriggerDecisionToken& token)
{
  // A token with run_number==0 and trigger_number==s_peer_announce_magic is a
  // DFO peer-announcement rather than a TRB completion token.
  if (token.run_number == 0 && token.trigger_number == s_peer_announce_magic) {
    TLOG_DEBUG(TLVL_PEER_ANNOUNCE) << get_name() << ": Received peer announcement from "
                                   << token.decision_destination;
    bool newly_registered = false;
    {
      std::lock_guard<std::mutex> guard(m_peers_mutex);
      auto [it, inserted] = m_registered_peers.insert(token.decision_destination);
      newly_registered = inserted;
    }
    m_peers_cv.notify_all();

    // If this is a genuinely new peer (e.g., a late joiner), recompute the
    // partition so the ensemble stays consistent.
    if (newly_registered) {
      compute_partition();
      ers::info(DFOConsensusPartitionInfo(ERS_HERE, get_name(), m_own_index.load(), m_num_dfos.load()));
    }
    return;
  }

  // All other tokens are regular TRB completion tokens.
  m_core->receive_token(token);
}

void
DFOConsensusModule::on_trigger_decision(const dfmessages::TriggerDecision& decision)
{
  // Buffer the TD for potential failover monitoring – all DFOs do this.
  {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds[decision.trigger_number] = { decision, std::chrono::steady_clock::now() };
  }

  size_t num_dfos = m_num_dfos.load();
  size_t own_index = m_own_index.load();

  // In multi-DFO mode, only the responsible DFO processes the decision.
  if (num_dfos > 1 && (decision.trigger_number % num_dfos) != own_index) {
    TLOG_DEBUG(TLVL_TD_FILTER) << get_name() << ": Buffered trigger_number " << decision.trigger_number
                               << " awaiting DFODecision from partition "
                               << (decision.trigger_number % num_dfos);
    return;
  }

  // Process via DFOCore; on_assignment callback will broadcast the DFODecision
  // and remove the entry from m_pending_tds.
  m_core->receive_trigger_decision(decision);
}

void
DFOConsensusModule::on_dfo_decision(const DFODecision& msg)
{
  TLOG_DEBUG(TLVL_DFO_DECISION) << get_name() << ": Received DFODecision from " << msg.source_dfo_name
                                 << " trigger=" << msg.trigger_number << " trb=" << msg.trb_connection_name
                                 << " slots=" << msg.trb_slot_count
                                 << " completion=" << std::boolalpha << msg.is_completion;

  // Update shadow slot count for the reporting DFO's TRB.
  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    m_remote_slot_counts[msg.source_dfo_name][msg.trb_connection_name] = msg.trb_slot_count;
  }

  // Remove the pending TD entry now that we know it was handled.
  if (!msg.is_completion) {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds.erase(msg.trigger_number);
  }

  // Recalculate and potentially update the inhibit signal.
  m_core->notify_trigger_if_needed();
}

// ---------------------------------------------------------------------------
// DFODecision broadcasting
// ---------------------------------------------------------------------------

void
DFOConsensusModule::broadcast_dfo_decision(daqdataformats::trigger_number_t trigger_number,
                                            const std::string& trb_conn,
                                            size_t trb_slot_count,
                                            bool is_completion)
{
  if (m_dfo_decision_output_connections.empty())
    return;

  DFODecision msg;
  msg.run_number = m_core->run_number();
  msg.trigger_number = trigger_number;
  msg.trb_connection_name = trb_conn;
  msg.trb_slot_count = trb_slot_count;
  msg.source_dfo_name = get_name();
  msg.is_completion = is_completion;

  auto iom = iomanager::IOManager::get();
  for (const auto& conn : m_dfo_decision_output_connections) {
    try {
      auto msg_copy = msg;
      iom->get_sender<DFODecision>(conn)->send(std::move(msg_copy), m_core->queue_timeout());
      TLOG_DEBUG(TLVL_DFO_DECISION) << get_name() << ": Sent DFODecision to " << conn
                                    << " trigger=" << trigger_number << " trb=" << trb_conn
                                    << " slots=" << trb_slot_count
                                    << " completion=" << std::boolalpha << is_completion;
    } catch (const ers::Issue& excpt) {
      ers::warning(excpt);
    }
  }
}

void
DFOConsensusModule::on_assignment(const std::shared_ptr<AssignedTriggerDecision>& atd,
                                   size_t trb_slot_count)
{
  // Broadcast to peer DFOs so they can update their shadow state.
  broadcast_dfo_decision(atd->decision.trigger_number, atd->connection_name, trb_slot_count, false);

  // Remove from the pending-TD buffer – this DFO has handled it.
  {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds.erase(atd->decision.trigger_number);
  }
}

void
DFOConsensusModule::on_completion(const std::string& trb_conn,
                                   daqdataformats::trigger_number_t trigger_number,
                                   size_t trb_slot_count)
{
  // Broadcast to peer DFOs so they can update their shadow state.
  broadcast_dfo_decision(trigger_number, trb_conn, trb_slot_count, true);
}

// ---------------------------------------------------------------------------
// Global busy check
// ---------------------------------------------------------------------------

bool
DFOConsensusModule::is_globally_busy() const
{
  if (m_core->num_trb_apps() == 0)
    return true; // No TRBs known yet – treat as busy.

  // Build aggregate slot count per TRB: own (from DFOCore) + peer (from shadow map).
  // If *any* TRB has available capacity the system is not globally busy.
  // Busy threshold comes from TriggerRecordBuilderData; we use DFOCore's own
  // is_busy() as the baseline for own slots and add peer slots on top.

  // Collect peer totals per TRB connection.
  std::map<std::string, size_t> peer_totals;
  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    for (const auto& [dfo_name, trb_map] : m_remote_slot_counts) {
      for (const auto& [trb_conn, count] : trb_map) {
        peer_totals[trb_conn] += count;
      }
    }
  }

  // Delegate to DFOCore, which knows the own slots and the busy_threshold per TRB.
  // We add peer_totals to each TRB's own count via DFOCore::is_globally_busy().
  return m_core->is_globally_busy(peer_totals);
}

// ---------------------------------------------------------------------------
// Watchdog / failover
// ---------------------------------------------------------------------------

void
DFOConsensusModule::watchdog_thread_func()
{
  TLOG_DEBUG(TLVL_WATCHDOG) << get_name() << ": Watchdog thread started";

  while (m_watchdog_running.load()) {
    std::this_thread::sleep_for(s_watchdog_interval);

    if (!m_watchdog_running.load())
      break;

    auto now = std::chrono::steady_clock::now();
    std::vector<std::pair<daqdataformats::trigger_number_t, PendingTD>> timed_out;

    {
      std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
      for (const auto& [tn, ptd] : m_pending_tds) {
        if ((now - ptd.received_at) >= s_dfo_decision_timeout) {
          timed_out.emplace_back(tn, ptd);
        }
      }
    }

    for (auto& [tn, ptd] : timed_out) {
      // Determine which DFO index was responsible for this trigger_number
      // based on the CURRENT partition (before failover removal).
      size_t num_dfos = m_num_dfos.load();
      size_t responsible_index = (num_dfos > 1) ? (tn % num_dfos) : 0;

      // Do not trigger failover if WE were responsible (should not happen, but
      // guard against it to avoid self-removal).
      if (responsible_index == m_own_index.load()) {
        TLOG_DEBUG(TLVL_WATCHDOG) << get_name() << ": Watchdog: own trigger " << tn
                                  << " still pending – TRBs may be saturated";
        continue;
      }

      handle_peer_failure(responsible_index, tn);
    }
  }

  TLOG_DEBUG(TLVL_WATCHDOG) << get_name() << ": Watchdog thread stopped";
}

void
DFOConsensusModule::handle_peer_failure(size_t failed_index,
                                         daqdataformats::trigger_number_t trigger_number)
{
  // Identify the failed DFO name from the sorted ensemble.
  std::string failed_dfo_name;
  {
    std::lock_guard<std::mutex> guard(m_peers_mutex);

    std::vector<std::string> ensemble;
    ensemble.push_back(get_name());
    for (const auto& peer : m_registered_peers) {
      ensemble.push_back(peer);
    }
    std::sort(ensemble.begin(), ensemble.end());

    if (failed_index >= ensemble.size())
      return; // Stale; partition has already changed.

    failed_dfo_name = ensemble[failed_index];
    if (failed_dfo_name == get_name())
      return; // Should not happen.

    m_registered_peers.erase(failed_dfo_name);
  }

  ers::warning(DFOConsensusFailover(ERS_HERE, get_name(), failed_dfo_name, trigger_number));

  // Recompute partition without the failed DFO.
  compute_partition();
  ers::info(DFOConsensusPartitionInfo(ERS_HERE, get_name(), m_own_index.load(), m_num_dfos.load()));

  // Also clear shadow slot counts from the failed DFO.
  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    m_remote_slot_counts.erase(failed_dfo_name);
  }

  // Re-assign all timed-out TDs that now belong to this DFO under the new partition.
  std::vector<dfmessages::TriggerDecision> to_reassign;
  {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    std::vector<daqdataformats::trigger_number_t> handled;
    for (auto& [tn, ptd] : m_pending_tds) {
      size_t new_num = m_num_dfos.load();
      size_t new_owner = (new_num > 1) ? (tn % new_num) : 0;
      if (new_owner == m_own_index.load()) {
        to_reassign.push_back(ptd.decision);
        handled.push_back(tn);
      }
    }
    for (auto tn : handled) {
      m_pending_tds.erase(tn);
    }
  }

  for (const auto& decision : to_reassign) {
    TLOG_DEBUG(TLVL_WATCHDOG) << get_name() << ": Failover: reassigning trigger_number "
                              << decision.trigger_number;
    m_core->receive_trigger_decision(decision);
    // on_assignment callback will broadcast DFODecision.
  }
}

} // namespace dunedaq::dfmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DFOConsensusModule)

