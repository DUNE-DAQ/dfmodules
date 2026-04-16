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

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "DFOModule" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5
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
  }
  for (auto con : mdal->get_outputs()) {
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerInhibit>()) {
      m_busy_sender = iom->get_sender<dfmessages::TriggerInhibit>(con->UID());
    }
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerDecision>()) {
      m_trb_conn_ids.push_back(con->UID());
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
  // Verify that receivers exist (fetches connection details eagerly)
  iom->get_receiver<dfmessages::TriggerDecisionToken>(m_token_connection);
  iom->get_receiver<dfmessages::TriggerDecision>(m_td_connection);

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

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method, there are "
                                      << m_core->num_trb_apps() << " TRB apps defined";
}

void
DFOModule::do_start(const CommandData_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  auto run_number = payload.value<daqdataformats::run_number_t>("run", 0);

  // 19-Dec-2024, KAB: check that TriggerDecision senders are ready to send at
  // start time so the ConnectivityService lookup happens here rather than at
  // the first send, avoiding delays that can cause spurious trigger inhibits.
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
    m_token_connection, std::bind(&DFOCore::receive_token, m_core.get(), std::placeholders::_1));

  iom->add_callback<dfmessages::TriggerDecision>(
    m_td_connection, std::bind(&DFOCore::receive_trigger_decision, m_core.get(), std::placeholders::_1));

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DFOModule::do_stop(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  auto iom = iomanager::IOManager::get();
  iom->remove_callback<dfmessages::TriggerDecision>(m_td_connection);

  auto remnants = m_core->stop();

  iom->remove_callback<dfmessages::TriggerDecisionToken>(m_token_connection);

  for (auto& r : remnants) {
    ers::error(IncompleteTriggerDecision(ERS_HERE, r->decision.trigger_number, m_core->run_number()));
  }

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

} // namespace dunedaq::dfmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DFOModule)
