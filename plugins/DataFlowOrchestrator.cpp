/**
 * @file DataFlowOrchestrator.cpp DataFlowOrchestrator class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DataFlowOrchestrator.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/app/Nljs.hpp"
#include "dfmodules/requestreceiver/Nljs.hpp"
#include "dfmodules/requestreceiver/Structs.hpp"
#include "dfmodules/requestreceiverinfo/InfoNljs.hpp"
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
#define TRACE_NAME "DataFlowOrchestrator" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10
};

namespace dunedaq {
namespace dfmodules {

DataFlowOrchestrator::DataFlowOrchestrator(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_queue_timeout(100)
  , m_run_number(0)
  , initial_tokens(1)
{
  register_command("conf", &DataFlowOrchestrator::do_conf);
  register_command("start", &DataFlowOrchestrator::do_start);
  register_command("stop", &DataFlowOrchestrator::do_stop);
  register_command("scrap", &DataFlowOrchestrator::do_scrap);
}

void
DataFlowOrchestrator::init(const data_t& init_data)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  //----------------------
  // Get queue
  //----------------------

  auto qi = appfwk::queue_index(init_data, { "trigger_decision_queue"} ) ;

  try {
    auto temp_info = qi["trigger_decision_queue"];
    std::string temp_name = temp_info.inst;
    m_data_request_queue.reset( new datareqsource_t(temp_name) );
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_input_queue", excpt);
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DataFlowOrchestrator::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  datafloworchestrator::ConfParams parsed_conf = payload.get<datafloworchestrator::ConfParams>();

  m_queue_timeout = std::chrono::milliseconds(parsed_conf.general_queue_timeout);

  m_initial_tokens = parsed_conf.initial_token_count ;

  m_td_connection_name = parsed_conf.td_connection ;
  m_token_connection_name = parsed_conf.token_connection ;

  networkmanager::NetworkManager::get().start_listening(m_token_connection_name);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
DataFlowOrchestrator::do_start(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  m_received_tokens = 0;
  m_run_number = payload.value<dunedaq::daqdataformats::run_number_t>("run", 0);

  networkmanager::NetworkManager::get().register_callback(
    m_token_connection_name, std::bind(&DataFlowOrchestrator::receive_tokens, this, std::placeholders::_1));

  m_is_running.store(true);

  std::async( std::launch::async, DataFlowOrchestrator::send_initial_trigger, this ) ;
  
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DataFlowOrchestrator::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  m_is_running.store(false);

  networkmanager::NetworkManager::get().clear_callback(m_token_connection_name);

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DataFlowOrchestrator::do_scrap(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  networkmanager::NetworkManager::get().stop_listening(m_token_connection_name);

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
DataFlowOrchestrator::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  datafloworchestratorinfo::Info info;
  info.requests_received = m_received_requests;
  ci.add(info);
}


void
DataFlowOrchestrator::send_initial_triggers() {

  for ( decltype(m_initial_tokens) i = 0;
	(i < m_initial_tokens) && m_is_running.load(); ) {

    dfmessages::TriggerDecision decision;
    if ( extract_a_decision(decision) ) {
      
      if ( dispatch( std::move(decision) ) ) {
	++i ;
      }
    }
  }

}


bool
DataFlowOrchestrator::extract_a_decision( dfmessages::DataRequest & decision ) {

  bool got_something = false;

  do {

    try {
      m_data_request_queue->pop(decision, m_queue_timeout);
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Popped the Trigger Decision with number "
                                  << decision.trigger_number
                                  << " off the input queue";
      got_something = true ;
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      // it is perfectly reasonable that there might be no data in the queue
      // some fraction of the times that we check, so we just continue on and try again
      continue;
    }

  } while( !got_something && m_is_running.load() );

  return got_something ;
   
}


void
DataFlowOrchestrator::receive_tokens( ipm::Receiver::Response message ) {
  
  auto token = serialization::deserialize<dfmessages::TriggerDecisionToken>(message.data);

  if ( token.run_number == m_run_number ) {

    dfmessages::TriggerDecision decision;
    if ( extract_a_decision(decision) ) {
      
      dispatch( std::move(decision) ) ;
    }

  }

}


bool
dispatch( dfmessages::TriggerDecision && decision ) {

  auto serialised_decision = dunedaq::serialization::serialize(decision, 
							       dunedaq::serialization::kMsgPack);

  bool wasSentSuccessfully = false;
  do {
    
    try {
      NetworkManager::get().send_to( m_td_connection_name, 
				     static_cast<const void*>(serialised_decision.data()),
				     serialised_decision.size(), 
				     m_queue_timeout );
      wasSentSuccessfully = true ;
    }
    catch (const ers::Issue& excpt) {
      std::ostringstream oss_warn;
      oss_warn << "Send to connection \"" << m_td_connection_name 
	       << "\" failed";
      ers::warning(networkmanager::OperationFailed(ERS_HERE, oss_warn.str(), excpt));
    }

  } while( !wasSentSuccessfully && m_is_running.load() );

  return wasSentSuccessfully;
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DataFlowOrchestrator)
