/**
 * @file FakeReqGen.cpp FakeReqGen class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FakeReqGen.hpp"
#include "CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/cmd/Nljs.hpp"
//#include "ddpdemo/fakereqgen/Nljs.hpp"

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
#define TRACE_NAME "FakeReqGen"    // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10 // NOLINT
#define TLVL_WORK_STEPS 15         // NOLINT

namespace dunedaq {
namespace ddpdemo {

FakeReqGen::FakeReqGen(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&FakeReqGen::do_work, this, std::placeholders::_1))
  , queueTimeout_(100)
  , triggerDecisionInputQueue_(nullptr)
  , triggerDecisionOutputQueue_(nullptr)
  , dataRequestOutputQueues_()
  , triggerInhibitOutputQueue_(nullptr)
{
  register_command("conf", &FakeReqGen::do_conf);
  register_command("start", &FakeReqGen::do_start);
  register_command("stop", &FakeReqGen::do_stop);
}

void
FakeReqGen::init(const data_t& init_data)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto qilist = appfwk::qindex(
    init_data, { "trigger_decision_input_queue", "trigger_decision_output_queue", "trigger_inhibit_output_queue" });
  try {
    triggerDecisionInputQueue_.reset(new trigdecsource_t(qilist["trigger_decision_input_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_input_queue", excpt);
  }
  try {
    triggerDecisionOutputQueue_.reset(new trigdecsink_t(qilist["trigger_decision_output_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_output_queue", excpt);
  }
  try {
    triggerInhibitOutputQueue_.reset(new triginhsink_t(qilist["trigger_inhibit_output_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_inhibit_output_queue", excpt);
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
FakeReqGen::do_conf(const data_t& /*payload*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  // fakereqgen::Conf tmpConfig = payload.get<fakereqgen::Conf>();
  // sleepMsecWhileRunning_ = tmpConfig.sleep_msec_while_running;

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
FakeReqGen::do_start(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  thread_.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
FakeReqGen::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  thread_.stop_working_thread();
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

    for (auto& dataReqQueue : dataRequestOutputQueues_) {
      dfmessages::DataRequest dataReq;
      dataReq.trigger_number = trigDecision.trigger_number;
      wasSentSuccessfully = false;
      while (!wasSentSuccessfully && running_flag.load()) {
        TLOG(TLVL_WORK_STEPS) << get_name() << ": Pushing the DataRequest for trigger number "
                              << dataReq.trigger_number << " onto an output queue";
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

    // TO-DO: add something here related to generating fake trigger inhibit messages

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

} // namespace ddpdemo
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::ddpdemo::FakeReqGen)
