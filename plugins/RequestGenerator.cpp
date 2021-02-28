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
#include "appfwk/app/Nljs.hpp"
#include "dfmodules/requestgenerator/Nljs.hpp"
#include "dfmodules/requestgenerator/Structs.hpp"
#include "logging/Logging.hpp"

#include <chrono>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief TRACE debug levels used in this source file
 */
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_WORK_STEPS = 10
};

namespace dunedaq {
namespace dfmodules {

RequestGenerator::RequestGenerator(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_thread(std::bind(&RequestGenerator::do_work, this, std::placeholders::_1))
  , m_queue_timeout(100)
  , m_trigger_decision_input_queue(nullptr)
  , m_trigger_decision_output_queue(nullptr)
{
  register_command("conf", &RequestGenerator::do_conf);
  register_command("start", &RequestGenerator::do_start);
  register_command("stop", &RequestGenerator::do_stop);
}

void
RequestGenerator::init(const data_t& init_data)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto qilist =
    appfwk::queue_index(init_data, { "trigger_decision_input_queue", "trigger_decision_for_event_building" });
  try {
    m_trigger_decision_input_queue.reset(new trigdecsource_t(qilist["trigger_decision_input_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_input_queue", excpt);
  }
  try {
    m_trigger_decision_output_queue.reset(new trigdecsink_t(qilist["trigger_decision_for_event_building"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_for_event_building", excpt);
  }

  auto ini = init_data.get<appfwk::app::ModInit>();
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
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  m_map_geoid_queues.clear();

  requestgenerator::ConfParams parsed_conf = payload.get<requestgenerator::ConfParams>();

  for (auto const& entry : parsed_conf.map) {
    dataformats::GeoID key;
    key.apa_number = entry.apa;
    key.link_number = entry.link;
    m_map_geoid_queues[key] = entry.queueinstance;
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
RequestGenerator::do_start(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  m_thread.start_working_thread();
  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
RequestGenerator::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  m_thread.stop_working_thread();
  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
RequestGenerator::do_work(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int32_t receivedCount = 0;
  // setting up the map <GeoID,data_req_queues>
  std::map<dataformats::GeoID, std::unique_ptr<datareqsink_t>> map;

  for (auto const& entry : m_map_geoid_queues) {
    map[entry.first] = std::unique_ptr<datareqsink_t>(new datareqsink_t(entry.second));
  }

  while (running_flag.load() || m_trigger_decision_input_queue->can_pop()) {
    dfmessages::TriggerDecision trigDecision;
    try {
      m_trigger_decision_input_queue->pop(trigDecision, m_queue_timeout);
      ++receivedCount;
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Popped the TriggerDecision for trigger number "
                                  << trigDecision.trigger_number << " off the input queue";
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      // it is perfectly reasonable that there might be no data in the queue
      // some fraction of the times that we check, so we just continue on and try again
      continue;
    }

    bool wasSentSuccessfully = false;
    do {
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Pushing the TriggerDecision for trigger number "
                                  << trigDecision.trigger_number << " onto the output queue";
      try {
        m_trigger_decision_output_queue->push(trigDecision, m_queue_timeout);
        wasSentSuccessfully = true;
      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
        std::ostringstream oss_warn;
        oss_warn << "push to output queue \"" << m_trigger_decision_output_queue->get_name() << "\"";
        ers::warning(dunedaq::appfwk::QueueTimeoutExpired(
          ERS_HERE,
          get_name(),
          oss_warn.str(),
          std::chrono::duration_cast<std::chrono::milliseconds>(m_queue_timeout).count()));
      }
    } while (!wasSentSuccessfully && running_flag.load());

    //-----------------------------------------
    // Loop over trigger decision components
    // Spawn each component_data_request to the corresponding link_data_handler_queue
    //----------------------------------------
    for (auto it = trigDecision.components.begin(); it != trigDecision.components.end(); it++) {
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": trigDecision.components.size :" << trigDecision.components.size();
      dfmessages::DataRequest dataReq;
      dataReq.trigger_number = trigDecision.trigger_number;
      dataReq.run_number = trigDecision.run_number;
      dataReq.trigger_timestamp = trigDecision.trigger_timestamp;

      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": trig_number " << dataReq.trigger_number << ": run_number "
                                  << dataReq.run_number << ": trig_timestamp " << dataReq.trigger_timestamp;

      dataformats::ComponentRequest comp_req = *it;
      dataformats::GeoID geoid_req = comp_req.component;
      dataReq.window_begin = comp_req.window_begin;
      dataReq.window_end = comp_req.window_end;

      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": apa_number " << geoid_req.apa_number << ": link_number "
                                  << geoid_req.link_number << ": window_begin " << comp_req.window_begin
                                  << ": window_end " << comp_req.window_end;

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
      do {
        TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Pushing the DataRequest from trigger number "
                                    << dataReq.trigger_number << " onto output queue :" << queue->get_name();

        // push data request into the corresponding queue
        try {
          queue->push(dataReq, m_queue_timeout);
          wasSentSuccessfully = true;
        } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
          std::ostringstream oss_warn;
          oss_warn << "push to output queue \"" << queue->get_name() << "\"";
          ers::warning(dunedaq::appfwk::QueueTimeoutExpired(
            ERS_HERE,
            get_name(),
            oss_warn.str(),
            std::chrono::duration_cast<std::chrono::milliseconds>(m_queue_timeout).count()));
        }
      } while (!wasSentSuccessfully && running_flag.load());
    }

    // TLOG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep while waiting for run Stop";
    // std::this_thread::sleep_for(std::chrono::milliseconds(m_sleep_msec_while_running));
    // TLOG(TLVL_WORK_STEPS) << get_name() << ": End of sleep while waiting for run Stop";
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, received Fake trigger decision messages for " << receivedCount
           << " triggers.";
  TLOG() << ProgressUpdate(ERS_HERE, get_name(), oss_summ.str());
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
} // NOLINT

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::RequestGenerator)
