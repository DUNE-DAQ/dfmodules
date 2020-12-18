/**
 * @file FakeFragRec.cpp FakeFragRec class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FakeFragRec.hpp"
#include "CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/cmd/Nljs.hpp"
//#include "ddpdemo/fakefragrec/Nljs.hpp"

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
#define TRACE_NAME "FakeFragRec"   // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10 // NOLINT
#define TLVL_WORK_STEPS 15         // NOLINT

namespace dunedaq {
namespace ddpdemo {

FakeFragRec::FakeFragRec(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&FakeFragRec::do_work, this, std::placeholders::_1))
  , queueTimeout_(100)
  , triggerDecisionInputQueue_(nullptr)
  , dataFragmentInputQueues_()
  , triggerRecordOutputQueue_(nullptr)
{
  register_command("conf", &FakeFragRec::do_conf);
  register_command("start", &FakeFragRec::do_start);
  register_command("stop", &FakeFragRec::do_stop);
}

void
FakeFragRec::init(const data_t& init_data)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto qilist = appfwk::qindex(init_data, { "trigger_decision_input_queue", "trigger_record_output_queue" });
  try {
    triggerDecisionInputQueue_.reset(new trigdecsource_t(qilist["trigger_decision_input_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_input_queue", excpt);
  }
  try {
    triggerRecordOutputQueue_.reset(new trigrecsink_t(qilist["trigger_record_output_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_record_output_queue", excpt);
  }
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";

  auto ini = init_data.get<appfwk::cmd::ModInit>();
  for (const auto& qitem : ini.qinfos) {
    if (qitem.name.rfind("data_fragment_") == 0) {
      try {
        dataFragmentInputQueues_.emplace_back(new datafragsource_t(qitem.inst));
      } catch (const ers::Issue& excpt) {
        throw InvalidQueueFatalError(ERS_HERE, get_name(), qitem.name, excpt);
      }
    }
  }
}

void
FakeFragRec::do_conf(const data_t& /*payload*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  // fakefragrec::Conf tmpConfig = payload.get<fakefragrec::Conf>();
  // sleepMsecWhileRunning_ = tmpConfig.sleep_msec_while_running;

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
FakeFragRec::do_start(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  thread_.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
FakeFragRec::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  thread_.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
FakeFragRec::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int32_t receivedTriggerCount = 0;
  int32_t receivedFragmentCount = 0;

  while (running_flag.load()) {
    dfmessages::TriggerDecision trigDecision;
    try {
      triggerDecisionInputQueue_->pop(trigDecision, queueTimeout_);
      ++receivedTriggerCount;

      std::unique_ptr<dataformats::TriggerRecord> trigRecPtr(new dataformats::TriggerRecord());
      trigRecPtr->set_trigger_number(trigDecision.trigger_number);
      bool wasSentSuccessfully = false;
      while (!wasSentSuccessfully && running_flag.load()) {
        TLOG(TLVL_WORK_STEPS) << get_name() << ": Pushing the Trigger Record for trigger number "
                              << trigRecPtr->get_trigger_number() << " onto the output queue";
        try {
          triggerRecordOutputQueue_->push(std::move(trigRecPtr), queueTimeout_);
          wasSentSuccessfully = true;
        } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
          std::ostringstream oss_warn;
          oss_warn << "push to output queue \"" << triggerRecordOutputQueue_->get_name() << "\"";
          ers::warning(dunedaq::appfwk::QueueTimeoutExpired(
            ERS_HERE,
            get_name(),
            oss_warn.str(),
            std::chrono::duration_cast<std::chrono::milliseconds>(queueTimeout_).count()));
        }
      }
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      // it is perfectly reasonable that there might be no data in the queue
      // some fraction of the times that we check, so we just continue on and try again
      // continue;
    }

    for (auto& dataFragQueue : dataFragmentInputQueues_) {
      std::unique_ptr<dataformats::Fragment> dataFragPtr;
      try {
        dataFragQueue->pop(dataFragPtr, queueTimeout_);
        ++receivedFragmentCount;
      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
        // it is perfectly reasonable that there might be no data in the queue
        // some fraction of the times that we check, so we just continue on and try again
        // continue;
      }
    }

    // TLOG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep while waiting for run Stop";
    // std::this_thread::sleep_for(std::chrono::milliseconds(sleepMsecWhileRunning_));
    // TLOG(TLVL_WORK_STEPS) << get_name() << ": End of sleep while waiting for run Stop";
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, received " << receivedTriggerCount
           << " Fake trigger decision messages and " << receivedFragmentCount << " Fake data fragmentss.";
  ers::info(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace ddpdemo
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::ddpdemo::FakeFragRec)
