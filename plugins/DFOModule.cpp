/**
 * @file DFOModule.cpp DFOModule class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DFOModule.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "dfmodules/opmon/DFOModule.pb.h"

#include "appmodel/DFOModule.hpp"
#include "confmodel/Connection.hpp"
#include "iomanager/IOManager.hpp"
#include "logging/Logging.hpp"

#include "trgdataformats/TriggerCandidateData.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "DFOModule" // NOLINT
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

DFOModule::DFOModule(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_core(std::make_unique<DFOCore>(name))
{
  register_command("conf", &DFOModule::do_conf);
  register_command("start", &DFOModule::do_start);
  register_command("drain_dataflow", &DFOModule::do_stop);
  register_command("scrap", &DFOModule::do_scrap);
}

void
DFOModule::init(std::shared_ptr<appfwk::ConfigurationManager> mcfg)
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
  m_consensus_enabled = m_dfo_conf->get_consensus_enabled();
  m_expected_peers = m_consensus_enabled ? m_dfo_decision_output_connections.size() : 0;

  iom->get_receiver<dfmessages::TriggerDecisionToken>(m_token_connection);
  iom->get_receiver<dfmessages::TriggerDecision>(m_td_connection);
  if (m_consensus_enabled && !m_dfo_decision_input_connection.empty()) {
    iom->get_receiver<DFODecision>(m_dfo_decision_input_connection);
  }

  TLOG() << get_name() << ": DFOModule initialized in " << (m_consensus_enabled ? "consensus" : "standalone")
         << " mode with " << m_expected_peers << " expected peer DFO(s)";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DFOModule::do_conf(const CommandData_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  m_core->configure(m_dfo_conf->get_busy_threshold(),
                    m_dfo_conf->get_free_threshold(),
                    m_dfo_conf->get_td_send_retries(),
                    std::chrono::milliseconds(m_dfo_conf->get_general_queue_timeout_ms()),
                    std::chrono::milliseconds(m_dfo_conf->get_stop_timeout_ms()));

  m_dfo_decision_timeout = std::chrono::milliseconds(m_dfo_conf->get_dfo_decision_timeout_ms());
  m_peer_announce_timeout = std::chrono::milliseconds(m_dfo_conf->get_peer_announce_timeout_ms());
  m_watchdog_interval = std::chrono::milliseconds(m_dfo_conf->get_watchdog_interval_ms());

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method, there are "
                                      << m_core->num_trb_apps() << " TRB apps defined";
}

void
DFOModule::do_start(const CommandData_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

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
  m_own_index.store(0);
  m_num_dfos.store(1);

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

  if (m_consensus_enabled) {
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
  } else {
    m_core->start(run_number,
                  m_busy_sender,
                  [iom](const std::string& conn) {
                    return iom->get_sender<dfmessages::TriggerDecision>(conn);
                  },
                  [this](const std::string& name, std::shared_ptr<TriggerRecordBuilderData> trbd) {
                    register_node(name, trbd);
                  });
  }

  iom->add_callback<dfmessages::TriggerDecisionToken>(
    m_token_connection, std::bind(&DFOModule::on_token, this, std::placeholders::_1));

  iom->add_callback<dfmessages::TriggerDecision>(
    m_td_connection, std::bind(&DFOModule::on_trigger_decision, this, std::placeholders::_1));

  if (m_consensus_enabled && !m_dfo_decision_input_connection.empty()) {
    iom->add_callback<DFODecision>(
      m_dfo_decision_input_connection,
      std::bind(&DFOModule::on_dfo_decision, this, std::placeholders::_1));
  }

  if (m_consensus_enabled) {
    send_peer_announcement();

    if (m_expected_peers > 0) {
      std::unique_lock<std::mutex> lock(m_peers_mutex);
      bool all_peers_ready = m_peers_cv.wait_for(lock, m_peer_announce_timeout, [this] {
        return m_registered_peers.size() >= m_expected_peers;
      });
      if (!all_peers_ready) {
        ers::warning(DFOConsensusPeerTimeout(
          ERS_HERE, get_name(), m_expected_peers, m_registered_peers.size()));
      }
    }

    compute_partition();
    ers::info(DFOConsensusPartitionInfo(ERS_HERE, get_name(), m_own_index.load(), m_num_dfos.load()));

    m_watchdog_running.store(true);
    m_watchdog_thread = std::thread(&DFOModule::watchdog_thread_func, this);
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DFOModule::do_stop(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  m_watchdog_running.store(false);
  if (m_watchdog_thread.joinable()) {
    m_watchdog_thread.join();
  }

  m_core->stop();

  auto iom = iomanager::IOManager::get();
  iom->remove_callback<dfmessages::TriggerDecision>(m_td_connection);

  auto remnants = m_core->flush();

  iom->remove_callback<dfmessages::TriggerDecisionToken>(m_token_connection);
  if (m_consensus_enabled && !m_dfo_decision_input_connection.empty()) {
    iom->remove_callback<DFODecision>(m_dfo_decision_input_connection);
  }

  for (auto& r : remnants) {
    ers::error(IncompleteTriggerDecision(ERS_HERE, r->decision.trigger_number, m_core->run_number()));
  }

  {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds.clear();
  }
  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    m_remote_slot_counts.clear();
  }

  m_own_index.store(0);
  m_num_dfos.store(1);

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DFOModule::do_scrap(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  m_core->scrap();

  TLOG() << get_name() << " successfully scrapped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
DFOModule::generate_opmon_data()
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

void
DFOModule::send_peer_announcement()
{
  if (!m_consensus_enabled || m_dfo_decision_output_connections.empty())
    return;

  DFODecision announcement;
  announcement.run_number = 0;
  announcement.trigger_number = s_peer_announce_magic;
  announcement.trb_connection_name = "";
  announcement.trb_slot_count = 0;
  announcement.source_dfo_name = get_name();
  announcement.is_completion = true;

  auto iom = iomanager::IOManager::get();
  for (const auto& conn : m_dfo_decision_output_connections) {
    try {
      auto announcement_copy = announcement;
      iom->get_sender<DFODecision>(conn)->send(
        std::move(announcement_copy), m_core->queue_timeout());
      TLOG_DEBUG(TLVL_PEER_ANNOUNCE) << get_name() << ": Sent peer announcement to " << conn;
    } catch (const ers::Issue& excpt) {
      ers::warning(excpt);
    }
  }
}

void
DFOModule::compute_partition()
{
  std::vector<std::string> ensemble;
  {
    std::lock_guard<std::mutex> guard(m_peers_mutex);
    ensemble.push_back(get_name());
    for (const auto& peer : m_registered_peers) {
      ensemble.push_back(peer);
    }
  }

  std::sort(ensemble.begin(), ensemble.end());

  auto it = std::find(ensemble.begin(), ensemble.end(), get_name());
  size_t own_index = (it != ensemble.end()) ? static_cast<size_t>(std::distance(ensemble.begin(), it)) : 0;

  m_own_index.store(own_index);
  m_num_dfos.store(ensemble.size());

  TLOG_DEBUG(TLVL_PARTITION) << get_name() << ": Partition computed: index=" << own_index << " of "
                              << ensemble.size() << " DFO(s)";
}

void
DFOModule::on_token(const dfmessages::TriggerDecisionToken& token)
{
  m_core->receive_token(token);
}

void
DFOModule::on_trigger_decision(const dfmessages::TriggerDecision& decision)
{
  if (!m_consensus_enabled) {
    m_core->receive_trigger_decision(decision);
    return;
  }

  {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds[decision.trigger_number] = { decision, std::chrono::steady_clock::now() };
  }

  size_t num_dfos = m_num_dfos.load();
  size_t own_index = m_own_index.load();

  if (num_dfos > 1 && (decision.trigger_number % num_dfos) != own_index) {
    TLOG_DEBUG(TLVL_TD_FILTER) << get_name() << ": Buffered trigger_number " << decision.trigger_number
                               << " awaiting DFODecision from partition "
                               << (decision.trigger_number % num_dfos);
    return;
  }

  m_core->receive_trigger_decision(decision);
}

void
DFOModule::on_dfo_decision(const DFODecision& msg)
{
  if (!m_consensus_enabled) {
    return;
  }

  TLOG_DEBUG(TLVL_DFO_DECISION) << get_name() << ": Received DFODecision from " << msg.source_dfo_name
                                 << " trigger=" << msg.trigger_number << " trb=" << msg.trb_connection_name
                                 << " slots=" << msg.trb_slot_count
                                 << " completion=" << std::boolalpha << msg.is_completion;

  if (msg.run_number == 0 && msg.trigger_number == s_peer_announce_magic) {
    TLOG_DEBUG(TLVL_PEER_ANNOUNCE) << get_name() << ": Received peer announcement from " << msg.source_dfo_name;
    bool newly_registered = false;
    {
      std::lock_guard<std::mutex> guard(m_peers_mutex);
      auto [it, inserted] = m_registered_peers.insert(msg.source_dfo_name);
      newly_registered = inserted;
    }
    m_peers_cv.notify_all();

    if (newly_registered) {
      compute_partition();
      ers::info(DFOConsensusPartitionInfo(ERS_HERE, get_name(), m_own_index.load(), m_num_dfos.load()));
    }
    return;
  }

  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    m_remote_slot_counts[msg.source_dfo_name][msg.trb_connection_name] = msg.trb_slot_count;
  }

  if (!msg.is_completion) {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds.erase(msg.trigger_number);
  }

  m_core->notify_trigger_if_needed();
}

void
DFOModule::broadcast_dfo_decision(daqdataformats::trigger_number_t trigger_number,
                                   const std::string& trb_conn,
                                   size_t trb_slot_count,
                                   bool is_completion)
{
  if (!m_consensus_enabled || m_dfo_decision_output_connections.empty())
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
DFOModule::on_assignment(const std::shared_ptr<AssignedTriggerDecision>& atd, size_t trb_slot_count)
{
  broadcast_dfo_decision(atd->decision.trigger_number, atd->connection_name, trb_slot_count, false);

  {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    m_pending_tds.erase(atd->decision.trigger_number);
  }
}

void
DFOModule::on_completion(const std::string& trb_conn,
                         daqdataformats::trigger_number_t trigger_number,
                         size_t trb_slot_count)
{
  broadcast_dfo_decision(trigger_number, trb_conn, trb_slot_count, true);
}

bool
DFOModule::is_globally_busy() const
{
  if (!m_consensus_enabled) {
    return m_core->is_busy();
  }

  if (m_core->num_trb_apps() == 0)
    return true;

  std::map<std::string, size_t> peer_totals;
  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    for (const auto& peer_slots : m_remote_slot_counts) {
      const auto& trb_map = peer_slots.second;
      for (const auto& [trb_conn, count] : trb_map) {
        peer_totals[trb_conn] += count;
      }
    }
  }

  return m_core->is_globally_busy(peer_totals);
}

void
DFOModule::watchdog_thread_func()
{
  TLOG_DEBUG(TLVL_WATCHDOG) << get_name() << ": Watchdog thread started";

  while (m_watchdog_running.load()) {
    std::this_thread::sleep_for(m_watchdog_interval);

    if (!m_watchdog_running.load())
      break;

    auto now = std::chrono::steady_clock::now();
    std::vector<std::pair<daqdataformats::trigger_number_t, PendingTD>> timed_out;

    {
      std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
      for (const auto& [tn, ptd] : m_pending_tds) {
        if ((now - ptd.received_at) >= m_dfo_decision_timeout) {
          timed_out.emplace_back(tn, ptd);
        }
      }
    }

    for (const auto& timed_out_td : timed_out) {
      auto tn = timed_out_td.first;
      size_t num_dfos = m_num_dfos.load();
      size_t own_index = m_own_index.load();
      size_t responsible_index = (num_dfos > 1) ? (tn % num_dfos) : 0;

      if (responsible_index == own_index) {
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
DFOModule::handle_peer_failure(size_t failed_index, daqdataformats::trigger_number_t trigger_number)
{
  std::string failed_dfo_name;
  {
    std::lock_guard<std::mutex> guard(m_peers_mutex);

    std::vector<std::string> ensemble;
    ensemble.push_back(get_name());
    for (const auto& peer : m_registered_peers) {
      ensemble.push_back(peer);
    }
    std::sort(ensemble.begin(), ensemble.end());

    if (failed_index >= ensemble.size()) {
      TLOG_DEBUG(TLVL_WATCHDOG) << get_name() << ": handle_peer_failure: stale failed_index=" << failed_index
                                << " (ensemble size=" << ensemble.size()
                                << "); partition already updated – skipping failover for trigger " << trigger_number;
      return;
    }

    failed_dfo_name = ensemble[failed_index];
    if (failed_dfo_name == get_name()) {
      TLOG_DEBUG(TLVL_WATCHDOG) << get_name() << ": handle_peer_failure: resolved failed_index=" << failed_index
                                << " to self – ignoring (trigger=" << trigger_number << ")";
      return;
    }

    m_registered_peers.erase(failed_dfo_name);
  }

  ers::warning(DFOConsensusFailover(ERS_HERE, get_name(), failed_dfo_name, trigger_number));

  compute_partition();
  ers::info(DFOConsensusPartitionInfo(ERS_HERE, get_name(), m_own_index.load(), m_num_dfos.load()));

  {
    std::lock_guard<std::mutex> guard(m_remote_slots_mutex);
    m_remote_slot_counts.erase(failed_dfo_name);
  }

  std::vector<dfmessages::TriggerDecision> to_reassign;
  {
    std::lock_guard<std::mutex> guard(m_pending_tds_mutex);
    std::vector<daqdataformats::trigger_number_t> handled;
    size_t new_num = m_num_dfos.load();
    size_t new_own = m_own_index.load();
    for (auto& [tn, ptd] : m_pending_tds) {
      size_t new_owner = (new_num > 1) ? (tn % new_num) : 0;
      if (new_owner == new_own) {
        to_reassign.push_back(ptd.decision);
        handled.push_back(tn);
      }
    }
    for (auto tn : handled) {
      m_pending_tds.erase(tn);
    }
  }

  for (const auto& decision : to_reassign) {
    TLOG_DEBUG(TLVL_WATCHDOG) << get_name() << ": Failover: reassigning trigger_number " << decision.trigger_number;
    m_core->receive_trigger_decision(decision);
  }
}

} // namespace dunedaq::dfmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DFOModule)
