/**
 * @file FakeTrigDecEmu.cpp FakeTrigDecEmu class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FakeTrigDecEmu.hpp"
#include "CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "ddpdemo/faketrigdecemu/Nljs.hpp"

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
#define TRACE_NAME "FakeTrigDecEmu" // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10  // NOLINT
#define TLVL_WORK_STEPS 15          // NOLINT

namespace dunedaq {
namespace ddpdemo {

FakeTrigDecEmu::FakeTrigDecEmu(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&FakeTrigDecEmu::do_work, this, std::placeholders::_1))
  , queueTimeout_(100)
  , triggerDecisionOutputQueue_(nullptr)
  , triggerInhibitInputQueue_(nullptr)
{
  register_command("conf", &FakeTrigDecEmu::do_conf);
  register_command("start", &FakeTrigDecEmu::do_start);
  register_command("stop", &FakeTrigDecEmu::do_stop);
}

void
FakeTrigDecEmu::init(const data_t& init_data)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto qi = appfwk::qindex(init_data, { "trigger_decision_output_queue", "trigger_inhibit_input_queue" });
  try {
    triggerDecisionOutputQueue_.reset(new trigdecsink_t(qi["trigger_decision_output_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_output_queue", excpt);
  }
  try {
    triggerInhibitInputQueue_.reset(new triginhsource_t(qi["trigger_inhibit_input_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_inhibit_input_queue", excpt);
  }
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
FakeTrigDecEmu::do_conf(const data_t& payload)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  faketrigdecemu::Conf tmpConfig = payload.get<faketrigdecemu::Conf>();
  sleepMsecWhileRunning_ = tmpConfig.sleep_msec_while_running;

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
FakeTrigDecEmu::do_start(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  thread_.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
FakeTrigDecEmu::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  thread_.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
FakeTrigDecEmu::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int32_t triggerCount = 0;

  while (running_flag.load()) {
    ++triggerCount;
    dfmessages::TriggerDecision trigDecision;
    trigDecision.trigger_number = triggerCount;

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

    // TO-DO: add something here for receiving Inhbit messages

    TLOG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep while waiting for run Stop";
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMsecWhileRunning_));
    TLOG(TLVL_WORK_STEPS) << get_name() << ": End of sleep while waiting for run Stop";
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, generated Fake trigger decision messages for " << triggerCount
           << " triggers.";
  ers::info(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace ddpdemo
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::ddpdemo::FakeTrigDecEmu)
