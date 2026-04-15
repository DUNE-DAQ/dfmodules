/**
 * @file DFOConsensusModule.cpp DFOConsensusModule class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DFOConsensusModule.hpp"

#include "appmodel/DFOModule.hpp"
#include "confmodel/Connection.hpp"
#include "iomanager/IOManager.hpp"
#include "logging/Logging.hpp"

#include <algorithm>
#include <limits>
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
  : DFOModule(name)
{
  // Override the start/stop command handlers with the consensus-aware versions.
  register_command("start", &DFOConsensusModule::do_start);
  register_command("drain_dataflow", &DFOConsensusModule::do_stop);
}

void
DFOConsensusModule::init(std::shared_ptr<appfwk::ConfigurationManager> mcfg)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  // Base-class init sets up TRB token input, TD input, busy-sender, and TRB
  // TD output connections.
  DFOModule::init(mcfg);

  // Additionally read any TriggerDecisionToken OUTPUT connections – these are
  // the peer DFO connections used to exchange peer-announcement tokens.
  auto mdal = mcfg->get_dal<appmodel::DFOModule>(get_name());
  for (auto con : mdal->get_outputs()) {
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerDecisionToken>()) {
      m_dfo_peer_output_connections.push_back(con->UID());
      TLOG_DEBUG(TLVL_PEER_ANNOUNCE) << get_name() << ": Found peer DFO output connection: " << con->UID();
    }
  }
  m_expected_peers = m_dfo_peer_output_connections.size();

  TLOG() << get_name() << ": DFOConsensusModule initialized with " << m_expected_peers << " expected peer DFO(s)";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
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

  // Call the base-class do_start, which registers the token and TD callbacks.
  // Because receive_trigger_complete_token and receive_trigger_decision are
  // now virtual, the overriding methods in this class will be invoked.
  DFOModule::do_start(payload);

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
DFOConsensusModule::do_stop(const CommandData_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  DFOModule::do_stop(payload);

  // Reset to standalone mode so a subsequent start is clean.
  m_own_index.store(0);
  m_num_dfos.store(1);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
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
      iom->get_sender<dfmessages::TriggerDecisionToken>(conn)->send(
        dfmessages::TriggerDecisionToken(announcement), m_queue_timeout);
      TLOG_DEBUG(TLVL_PEER_ANNOUNCE) << get_name() << ": Sent peer announcement to " << conn;
    } catch (const ers::Issue& excpt) {
      std::ostringstream oss;
      oss << "Could not send peer announcement to " << conn;
      ers::warning(iomanager::OperationFailed(ERS_HERE, oss.str(), excpt));
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

  TLOG_DEBUG(TLVL_PARTITION) << get_name() << ": Partition computed: index=" << own_index
                              << " of " << ensemble.size() << " DFO(s)";
}

void
DFOConsensusModule::receive_trigger_complete_token(const dfmessages::TriggerDecisionToken& token)
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

  // All other tokens are handled by the base class.
  DFOModule::receive_trigger_complete_token(token);
}

void
DFOConsensusModule::receive_trigger_decision(const dfmessages::TriggerDecision& decision)
{
  size_t num_dfos = m_num_dfos.load();
  size_t own_index = m_own_index.load();

  // In multi-DFO mode, only process the decisions that belong to our partition.
  if (num_dfos > 1 && (decision.trigger_number % num_dfos) != own_index) {
    TLOG_DEBUG(TLVL_TD_FILTER) << get_name() << ": Skipping trigger_number " << decision.trigger_number
                                << " (belongs to partition "
                                << (decision.trigger_number % num_dfos)
                                << ", own index is " << own_index << ")";
    return;
  }

  // This decision belongs to our partition – hand off to base-class processing.
  DFOModule::receive_trigger_decision(decision);
}

} // namespace dunedaq::dfmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DFOConsensusModule)
