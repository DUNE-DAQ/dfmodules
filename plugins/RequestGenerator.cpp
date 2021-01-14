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
#include "dfmodules/requestgenerator/Nljs.hpp"
#include "dfmodules/requestgenerator/Structs.hpp"

#include "TRACE/trace.h"
#include "ers/ers.h"

#include <chrono>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>
/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "RequestGenerator"          // NOLINT
#define TLVL_ENTER_EXIT_METHODS TLVL_DEBUG + 5 // NOLINT
#define TLVL_WORK_STEPS TLVL_DEBUG + 10        // NOLINT

namespace dunedaq {
namespace dfmodules {

RequestGenerator::RequestGenerator(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&RequestGenerator::do_work, this, std::placeholders::_1))
  , queueTimeout_(100)
  , triggerDecisionInputQueue_(nullptr)
  , triggerDecisionOutputQueue_(nullptr)
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
        datareqsink_t temp(qitem.inst);
      } catch (const ers::Issue& excpt) {
        throw InvalidQueueFatalError(ERS_HERE, get_name(), qitem.name, excpt);
      }
    }
  }
}

void
RequestGenerator::do_conf(const data_t& payload)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  m_map_geoid_queues.clear();

  requestgenerator::ConfParams parsed_conf = payload.get<requestgenerator::ConfParams>();

  for (auto const& entry : parsed_conf.map) {
    dataformats::GeoID key;
    key.apa_number = entry.apa;
    key.link_number = entry.link;
    m_map_geoid_queues[key] = entry.queueinstance;
  }

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
  // setting up the map <GeoID,data_req_queues>
  std::map<dataformats::GeoID, std::unique_ptr<datareqsink_t>> map;

  for (auto const& entry : m_map_geoid_queues) {
    map[entry.first] = std::unique_ptr<datareqsink_t>(new datareqsink_t(entry.second));
  }

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
    // Loop over trigger decision components
    // Spawn each component_data_request to the corresponding link_data_handler_queue
    //----------------------------------------
    for (auto it = trigDecision.components.begin(); it != trigDecision.components.end(); it++) {
      TLOG(TLVL_WORK_STEPS) << get_name() << ": trigDecision.components.size :" << trigDecision.components.size();
      dfmessages::DataRequest dataReq;
      dataReq.trigger_number = trigDecision.trigger_number;
      dataReq.run_number = trigDecision.run_number;
      dataReq.trigger_timestamp = trigDecision.trigger_timestamp;

      TLOG(TLVL_WORK_STEPS) << get_name() << ": trig_number " << dataReq.trigger_number << ": run_number "
                            << dataReq.run_number << ": trig_timestamp " << dataReq.trigger_timestamp;

      dataformats::ComponentRequest comp_req = it->second;
      dataformats::GeoID geoid_req = it->first;
      dataReq.window_offset = comp_req.window_offset;
      dataReq.window_width = comp_req.window_width;

      TLOG(TLVL_WORK_STEPS) << get_name() << ": apa_number " << geoid_req.apa_number << ": link_number "
                            << geoid_req.link_number << ": window_offset " << comp_req.window_offset
                            << ": window_width " << comp_req.window_width;

      // find the queue for geoid_req in the map
      auto it_req = map.find(geoid_req);
      if (it_req == map.end()) {
        // if geoid request is not valid. then trhow error and continue
        ers::error(dunedaq::dfmodules::UnknownGeoID(
          ERS_HERE, dataReq.trigger_number, dataReq.run_number, geoid_req.apa_number, geoid_req.link_number));
        continue;
      }

      // get the queue from map element
      auto& queue = it_req->second;

      wasSentSuccessfully = false;
      while (!wasSentSuccessfully && running_flag.load()) {

        TLOG(TLVL_WORK_STEPS) << get_name() << ": Pushing the DataRequest from trigger number "
                              << dataReq.trigger_number << " onto output queue :" << queue->get_name();

        // push data request into the corresponding queue
        try {
          queue->push(dataReq, queueTimeout_);
          wasSentSuccessfully = true;
        } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
          std::ostringstream oss_warn;
          oss_warn << "push to output queue \"" << queue->get_name() << "\"";
          ers::warning(dunedaq::appfwk::QueueTimeoutExpired(
            ERS_HERE,
            get_name(),
            oss_warn.str(),
            std::chrono::duration_cast<std::chrono::milliseconds>(queueTimeout_).count()));
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
  ers::log(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
} // NOLINT

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::RequestGenerator)
