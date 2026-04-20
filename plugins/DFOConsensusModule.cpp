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
  TLVL_TD_FILTER = 10
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

  TLOG() << get_name() << ": DFOConsensusModule initialized with " << m_expected_peers << " expected peer DFO(s)";
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
                });

  iom->add_callback<dfmessages::TriggerDecisionToken>(
    m_token_connection, std::bind(&DFOConsensusModule::on_token, this, std::placeholders::_1));

  iom->add_callback<dfmessages::TriggerDecision>(
    m_td_connection, std::bind(&DFOConsensusModule::on_trigger_decision, this, std::placeholders::_1));

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

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DFOConsensusModule::do_stop(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  m_core->stop();

  auto iom = iomanager::IOManager::get();
  iom->remove_callback<dfmessages::TriggerDecision>(m_td_connection);

  auto remnants = m_core->flush();

  iom->remove_callback<dfmessages::TriggerDecisionToken>(m_token_connection);

  for (auto& r : remnants) {
    ers::error(IncompleteTriggerDecision(ERS_HERE, r->decision.trigger_number, m_core->run_number()));
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
  size_t num_dfos = m_num_dfos.load();
  size_t own_index = m_own_index.load();

  // In multi-DFO mode, only process decisions that belong to our partition.
  if (num_dfos > 1 && (decision.trigger_number % num_dfos) != own_index) {
    TLOG_DEBUG(TLVL_TD_FILTER) << get_name() << ": Skipping trigger_number " << decision.trigger_number
                               << " (belongs to partition " << (decision.trigger_number % num_dfos)
                               << ", own index is " << own_index << ")";
    return;
  }

  m_core->receive_trigger_decision(decision);
}

} // namespace dunedaq::dfmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DFOConsensusModule)
