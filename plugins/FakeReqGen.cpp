/**
 * @file FakeReqGen.cpp FakeReqGen class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FakeReqGen.hpp"
#include "dfmodules/CommonIssues.hpp"
//#include "dfmodules/fakereqgen/Nljs.hpp"

#include "TRACE/trace.h"
#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/cmd/Nljs.hpp"
#include "ers/ers.h"

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
#define TRACE_NAME "FakeReqGen"                // NOLINT
#define TLVL_ENTER_EXIT_METHODS TLVL_DEBUG + 5 // NOLINT
#define TLVL_WORK_STEPS TLVL_DEBUG + 10        // NOLINT

namespace dunedaq {
namespace dfmodules {

FakeReqGen::FakeReqGen(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_thread(std::bind(&FakeReqGen::do_work, this, std::placeholders::_1))
  , m_queue_timeout(100)
  , m_trigger_decision_input_queue(nullptr)
  , m_trigger_decision_output_queue(nullptr)
  , m_data_request_output_queues()
{
  register_command("conf", &FakeReqGen::do_conf);
  register_command("start", &FakeReqGen::do_start);
  register_command("stop", &FakeReqGen::do_stop);
}

void
FakeReqGen::init(const data_t& init_data)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto qilist = appfwk::queue_index(
    init_data,
    { "trigger_decision_input_queue", "trigger_decision_for_event_building", "trigger_decision_for_inhibit" });
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
  try {
    std::unique_ptr<trigdecsink_t> trig_dec_queue_for_inh;
    trig_dec_queue_for_inh.reset(new trigdecsink_t(qilist["trigger_decision_for_inhibit"].inst));
    m_trigger_decision_forwarder.reset(new TriggerDecisionForwarder(get_name(), std::move(trig_dec_queue_for_inh)));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_for_inhibit", excpt);
  }

  auto ini = init_data.get<appfwk::cmd::ModInit>();
  for (const auto& qitem : ini.qinfos) {
    if (qitem.name.rfind("data_request_") == 0) {
      try {
        m_data_request_output_queues.emplace_back(new datareqsink_t(qitem.inst));
      } catch (const ers::Issue& excpt) {
        throw InvalidQueueFatalError(ERS_HERE, get_name(), qitem.name, excpt);
      }
    }
  }
}

void
FakeReqGen::do_conf(const data_t& /*payload*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  // fakereqgen::Conf tmpConfig = payload.get<fakereqgen::Conf>();
  // m_sleep_msec_while_running = tmpConfig.sleep_msec_while_running;

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
FakeReqGen::do_start(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  m_trigger_decision_forwarder->start_forwarding();
  m_thread.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
FakeReqGen::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  m_trigger_decision_forwarder->stop_forwarding();
  m_thread.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
FakeReqGen::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int32_t receivedCount = 0;

  while (running_flag.load()) {
    dfmessages::TriggerDecision trigDecision;
    try {
      m_trigger_decision_input_queue->pop(trigDecision, m_queue_timeout);
      ++receivedCount;
      TLOG(TLVL_WORK_STEPS) << get_name() << ": Popped the TriggerDecision for trigger number "
                            << trigDecision.m_trigger_number << " off the input queue";
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      // it is perfectly reasonable that there might be no data in the queue
      // some fraction of the times that we check, so we just continue on and try again
      continue;
    }

    bool wasSentSuccessfully = false;
    while (!wasSentSuccessfully && running_flag.load()) {
      TLOG(TLVL_WORK_STEPS) << get_name() << ": Pushing the TriggerDecision for trigger number "
                            << trigDecision.m_trigger_number << " onto the output queue";
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
    }

    for (auto& dataReqQueue : m_data_request_output_queues) {
      dfmessages::DataRequest dataReq;
      dataReq.m_trigger_number = trigDecision.m_trigger_number;
      dataReq.m_run_number = trigDecision.m_run_number;
      dataReq.m_trigger_timestamp = trigDecision.m_trigger_timestamp;

      // hack: only use the request window from one of the components
      auto first_map_element = trigDecision.m_components.begin();
      if (first_map_element != trigDecision.m_components.end()) {
        dataformats::ComponentRequest comp_req = first_map_element->second;
        dataReq.m_window_offset = comp_req.m_window_offset;
        dataReq.m_window_width = comp_req.m_window_width;
      } else {
        dataReq.m_window_offset = 0x123456789abcdef0; // placeholder
        dataReq.m_window_width = 0x123456789abcdef0;  // placeholder
      }

      wasSentSuccessfully = false;
      while (!wasSentSuccessfully && running_flag.load()) {
        TLOG(TLVL_WORK_STEPS) << get_name() << ": Pushing the DataRequest for trigger number "
                              << dataReq.m_trigger_number << " onto an output queue";
        try {
          dataReqQueue->push(dataReq, m_queue_timeout);
          wasSentSuccessfully = true;
        } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
          std::ostringstream oss_warn;
          oss_warn << "push to output queue \"" << dataReqQueue->get_name() << "\"";
          ers::warning(dunedaq::appfwk::QueueTimeoutExpired(
            ERS_HERE,
            get_name(),
            oss_warn.str(),
            std::chrono::duration_cast<std::chrono::milliseconds>(m_queue_timeout).count()));
        }
      }
    }

    m_trigger_decision_forwarder->set_latest_trigger_decision(trigDecision);

    // TLOG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep while waiting for run Stop";
    // std::this_thread::sleep_for(std::chrono::milliseconds(m_sleep_msec_while_running));
    // TLOG(TLVL_WORK_STEPS) << get_name() << ": End of sleep while waiting for run Stop";
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, received Fake trigger decision messages for " << receivedCount
           << " triggers.";
  ers::log(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::FakeReqGen)
