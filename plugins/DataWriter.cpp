/**
 * @file DataWriter.cpp DataWriter class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DataWriter.hpp"
#include "CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "dfmodules/datawriter/Nljs.hpp"

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
#define TRACE_NAME "DataWriter"    // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10 // NOLINT
#define TLVL_CONFIG 12             // NOLINT
#define TLVL_WORK_STEPS 15         // NOLINT

namespace dunedaq {
namespace dfmodules {

DataWriter::DataWriter(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&DataWriter::do_work, this, std::placeholders::_1))
  , queueTimeout_(100)
  , triggerRecordInputQueue_(nullptr)
{
  register_command("conf", &DataWriter::do_conf);
  register_command("start", &DataWriter::do_start);
  register_command("stop", &DataWriter::do_stop);
}

void
DataWriter::init(const data_t& init_data)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto qi = appfwk::qindex(init_data, { "trigger_record_input_queue" });
  try {
    triggerRecordInputQueue_.reset(new trigrecsource_t(qi["trigger_record_input_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_record_input_queue", excpt);
  }
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DataWriter::do_conf(const data_t& payload)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  datawriter::ConfParams tmpConfig = payload.get<datawriter::ConfParams>();
  TLOG(TLVL_CONFIG) << get_name() << ": operational mode = " << tmpConfig.mode;

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
DataWriter::do_start(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  thread_.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DataWriter::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  thread_.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DataWriter::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int32_t receivedCount = 0;

  while (running_flag.load()) {
    std::unique_ptr<dataformats::TriggerRecord> trigRecPtr;

    try {
      triggerRecordInputQueue_->pop(trigRecPtr, queueTimeout_);
      ++receivedCount;
      TLOG(TLVL_WORK_STEPS) << get_name() << ": Popped the TriggerRecord for trigger number "
                            << trigRecPtr->get_trigger_number() << " off the input queue";
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      // it is perfectly reasonable that there might be no data in the queue
      // some fraction of the times that we check, so we just continue on and try again
      continue;
    }

    // TLOG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep while waiting for run Stop";
    // std::this_thread::sleep_for(std::chrono::milliseconds(sleepMsecWhileRunning_));
    // TLOG(TLVL_WORK_STEPS) << get_name() << ": End of sleep while waiting for run Stop";
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, received  trigger record messages for " << receivedCount << " triggers.";
  ers::info(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DataWriter)
