/**
 * @file TriggerDecisionReceiver.cpp TriggerDecisionReceiver class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "TriggerDecisionReceiver.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/app/Nljs.hpp"
//#include "dfmessages/TriggerDecision_serialization.hpp"
#include "dfmodules/triggerdecisionreceiver/Nljs.hpp"
#include "dfmodules/triggerdecisionreceiver/Structs.hpp"
#include "dfmodules/triggerdecisionreceiverinfo/InfoNljs.hpp"
#include "logging/Logging.hpp"
#include "networkmanager/NetworkManager.hpp"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "TriggerDecisionReceiver" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10
};

namespace dunedaq {
namespace dfmodules {

TriggerDecisionReceiver::TriggerDecisionReceiver(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_queue_timeout(100)
  , m_run_number(0)
{
  register_command("conf", &TriggerDecisionReceiver::do_conf);
  register_command("start", &TriggerDecisionReceiver::do_start);
  register_command("stop", &TriggerDecisionReceiver::do_stop);
}

void
TriggerDecisionReceiver::init(const data_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  auto qi = appfwk::queue_index(init_data, {"output"} ) ;

  m_triggerdecision_output_queue =
    std::unique_ptr<triggerdecisionsink_t>(new triggerdecisionsink_t(qi["output"].inst));  

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
TriggerDecisionReceiver::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";
  triggerdecisionreceiver::ConfParams parsed_conf = payload.get<triggerdecisionreceiver::ConfParams>();

  m_queue_timeout = std::chrono::milliseconds(parsed_conf.general_queue_timeout);
  m_connection_name = parsed_conf.connection_name;

  networkmanager::NetworkManager::get().start_listening(m_connection_name);
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
TriggerDecisionReceiver::do_start(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  m_received_triggerdecisions = 0;
  m_run_number = payload.value<dunedaq::dataformats::run_number_t>("run", 0);

  networkmanager::NetworkManager::get().register_callback(
    m_connection_name, std::bind(&TriggerDecisionReceiver::dispatch_triggerdecision, this, std::placeholders::_1));

  TLOG() << get_name() << " successfully started for run number " << m_run_number;
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
TriggerDecisionReceiver::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  networkmanager::NetworkManager::get().stop_listening(m_connection_name);

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
TriggerDecisionReceiver::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  triggerdecisionreceiverinfo::Info info;
  info.triggerdecisions_received = m_received_triggerdecisions;
  ci.add(info);
}

void
TriggerDecisionReceiver::dispatch_triggerdecision(ipm::Receiver::Response message)
{
  auto triggerdecision = serialization::deserialize<dfmessages::TriggerDecision>(message.data);
  m_triggerdecision_output_queue->push(std::move(triggerdecision), m_queue_timeout);
  m_received_triggerdecisions++;
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::TriggerDecisionReceiver)
