/**
 * @file RequestGenerator.cpp RequestGenerator class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "RequestGenerator.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/cmd/Nljs.hpp"
//#include "dfmodules/fakereqgen/Nljs.hpp"

#include "TRACE/trace.h"
#include "ers/ers.h"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "RequestGenerator"    // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10 // NOLINT
#define TLVL_WORK_STEPS 15         // NOLINT

namespace dunedaq {
namespace dfmodules {

RequestGenerator::RequestGenerator(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&RequestGenerator::do_work, this, std::placeholders::_1))
  , queueTimeout_(100)
  , triggerDecisionInputQueue_(nullptr)
  , triggerDecisionOutputQueue_(nullptr)
  , dataRequestOutputQueues_()
{
  register_command("conf", &RequestGenerator::do_conf);
  register_command("start", &RequestGenerator::do_start);
  register_command("stop", &RequestGenerator::do_stop);
}

void
RequestGenerator::init(const data_t& init_data)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto qilist = appfwk::qindex(
    init_data,
    { "trigger_decision_input_queue", "trigger_decision_for_event_building", "trigger_decision_for_inhibit" });
  try {
    triggerDecisionInputQueue_.reset(new trigdecsource_t(qilist["trigger_decision_input_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_input_queue", excpt);
  }
  try {
    triggerDecisionOutputQueue_.reset(new trigdecsink_t(qilist["trigger_decision_for_event_building"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_for_event_building", excpt);
  }
  try {
    std::unique_ptr<trigdecsink_t> trig_dec_queue_for_inh;
    trig_dec_queue_for_inh.reset(new trigdecsink_t(qilist["trigger_decision_for_inhibit"].inst));
    trigger_decision_forwarder_.reset(new TriggerDecisionForwarder(get_name(), std::move(trig_dec_queue_for_inh)));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_for_inhibit", excpt);
  }

  auto ini = init_data.get<appfwk::cmd::ModInit>();
  for (const auto& qitem : ini.qinfos) {
    if (qitem.name.rfind("data_request_") == 0) {
      try {
        dataRequestOutputQueues_.emplace_back(new datareqsink_t(qitem.inst));
      } catch (const ers::Issue& excpt) {
        throw InvalidQueueFatalError(ERS_HERE, get_name(), qitem.name, excpt);
      }
    }
  }
}

void
RequestGenerator::do_conf(const data_t& /*payload*/)
//RequestGenerator::do_conf(const nlohmann::json& confobj /*payload*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";
  // fakereqgen::Conf tmpConfig = payload.get<fakereqgen::Conf>();
  // sleepMsecWhileRunning_ = tmpConfig.sleep_msec_while_running;
  //  auto params=confobj.get<requestgenerator::ConfParams>();

  //  for(auto const& link_queue: params.links){
    // TODO: Set APA properly                                                                                                                  
  //    map_links_queues_.insert(link_queue->first,);
  //  }

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
RequestGenerator::do_start(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  trigger_decision_forwarder_->start_forwarding();
  thread_.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
RequestGenerator::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  trigger_decision_forwarder_->stop_forwarding();
  thread_.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
RequestGenerator::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int32_t receivedCount = 0;

  while (running_flag.load()) {
    dfmessages::TriggerDecision trigDecision;
    try {
      triggerDecisionInputQueue_->pop(trigDecision, queueTimeout_);
      ++receivedCount;
      TLOG(TLVL_WORK_STEPS) << get_name() << ": Popped the TriggerDecision for trigger number "
                            << trigDecision.trigger_number << " off the input queue";
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      // it is perfectly reasonable that there might be no data in the queue
      // some fraction of the times that we check, so we just continue on and try again
      continue;
    }

    bool wasSentSuccessfully = false;
    while (!wasSentSuccessfully && running_flag.load()) {
      TLOG(TLVL_WORK_STEPS) << get_name() << ": Pushing the TriggerDecision for trigger number "
                            << trigDecision.trigger_number << " onto the output queue";
      try {
        triggerDecisionOutputQueue_->push(trigDecision, queueTimeout_);
        wasSentSuccessfully = true;
      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
        std::ostringstream oss_warn;
        oss_warn << "push to output queue \"" << triggerDecisionOutputQueue_->get_name() << "\"";
        ers::warning(dunedaq::appfwk::QueueTimeoutExpired(
          ERS_HERE,
          get_name(),
          oss_warn.str(),
          std::chrono::duration_cast<std::chrono::milliseconds>(queueTimeout_).count()));
      }
    }

    //-----------------------------------------
    //Loop over trigger decision components 
    //Spawn each component_data_request to the corresponding link_data_handler_queue 
    //----------------------------------------
    for ( auto it = trigDecision.components.begin() ; it != trigDecision.components.end() ; it++) {
      TLOG(TLVL_WORK_STEPS) << get_name() << ": trigDecision.components.size :"<<  trigDecision.components.size();
      dfmessages::DataRequest dataReq;
      dataReq.trigger_number = trigDecision.trigger_number;
      dataReq.run_number = trigDecision.run_number;
      dataReq.trigger_timestamp = trigDecision.trigger_timestamp;

      TLOG(TLVL_WORK_STEPS) << get_name() << ": trig_number : "<<dataReq.trigger_number 
                            <<"  run_number : "<< dataReq.run_number 
                            << "  trig_timestamp " << dataReq.trigger_timestamp ; 

      dataformats::ComponentRequest comp_req = it->second;
      dataformats::GeoID geoid_req = it->first ; 
      dataReq.window_offset = comp_req.window_offset;
      dataReq.window_width = comp_req.window_width;

      
      std::string requestQueue = "data_requests_" + std::to_string(geoid_req.link_number); 
      TLOG(TLVL_WORK_STEPS) << get_name() << ": APA : "<<geoid_req.apa_number 
                            <<"  Link_num : " << geoid_req.link_number 
                            << " window_offset " << comp_req.window_offset  
                            << " window_width " << comp_req.window_width 
                            << " requestQueue : " << requestQueue; 


      //Looping over the queues as a hack while we get the mapping of configuration working properly
      for ( auto& dataReqQueue : dataRequestOutputQueues_ ) {

	if ( requestQueue.compare( dataReqQueue->get_name() ) ) continue ; // 
	  
	TLOG(TLVL_WORK_STEPS) << get_name() << ": dataReqQueue->get_name() : " 
                              << dataReqQueue->get_name() << "  requestQueue : "<<requestQueue ;
	
	wasSentSuccessfully = false;
	while (!wasSentSuccessfully && running_flag.load()) {
	  TLOG(TLVL_WORK_STEPS) << get_name() << ": Pushing the DataRequest for trigger number " 
                                << dataReq.trigger_number
				<< " onto an output queue";	    
	  try {
	    dataReqQueue->push(dataReq, queueTimeout_);
	    wasSentSuccessfully = true;
	  } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
	    std::ostringstream oss_warn;
	    oss_warn << "push to output queue \"" << dataReqQueue->get_name() << "\"";
	    ers::warning(dunedaq::appfwk::QueueTimeoutExpired(
              ERS_HERE,
	      get_name(),
	      oss_warn.str(),
	      std::chrono::duration_cast<std::chrono::milliseconds>(queueTimeout_).count()));
	  }
	}
      }
    }
 
    trigger_decision_forwarder_->set_latest_trigger_decision(trigDecision);

    // TLOG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep while waiting for run Stop";
    // std::this_thread::sleep_for(std::chrono::milliseconds(sleepMsecWhileRunning_));
    // TLOG(TLVL_WORK_STEPS) << get_name() << ": End of sleep while waiting for run Stop";
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, received Fake trigger decision messages for " << receivedCount
           << " triggers.";
  ers::info(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::RequestGenerator)
