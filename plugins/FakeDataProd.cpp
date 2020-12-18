/**
 * @file FakeDataProd.cpp FakeDataProd class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FakeDataProd.hpp"
#include "CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
//#include "ddpdemo/fakedataprod/Nljs.hpp"

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
#define TRACE_NAME "FakeDataProd"  // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10 // NOLINT
#define TLVL_WORK_STEPS 15         // NOLINT

namespace dunedaq {
namespace ddpdemo {

FakeDataProd::FakeDataProd(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&FakeDataProd::do_work, this, std::placeholders::_1))
  , queueTimeout_(100)
  , dataRequestInputQueue_(nullptr)
  , dataFragmentOutputQueue_(nullptr)
{
  register_command("conf", &FakeDataProd::do_conf);
  register_command("start", &FakeDataProd::do_start);
  register_command("stop", &FakeDataProd::do_stop);
}

void
FakeDataProd::init(const data_t& init_data)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto qi = appfwk::qindex(init_data, { "data_request_input_queue", "data_fragment_output_queue" });
  try {
    dataRequestInputQueue_.reset(new datareqsource_t(qi["data_request_input_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "data_request_input_queue", excpt);
  }
  try {
    dataFragmentOutputQueue_.reset(new datafragsink_t(qi["data_fragment_output_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "data_fragment_output_queue", excpt);
  }
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
FakeDataProd::do_conf(const data_t& /*payload*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  // fakedataprod::Conf tmpConfig = payload.get<fakedataprod::Conf>();
  // sleepMsecWhileRunning_ = tmpConfig.sleep_msec_while_running;

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
FakeDataProd::do_start(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  thread_.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
FakeDataProd::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  thread_.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
FakeDataProd::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int32_t receivedCount = 0;

  while (running_flag.load()) {
    dfmessages::DataRequest dataReq;
    try {
      dataRequestInputQueue_->pop(dataReq, queueTimeout_);
      ++receivedCount;
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      // it is perfectly reasonable that there might be no data in the queue
      // some fraction of the times that we check, so we just continue on and try again
      continue;
    }

    // TODO PAR 2020-12-17: dataformats::Fragment has to be
    // constructed with some payload data, so I'm putting a single int
    // in it for now
    int dummy_int=3;
    std::unique_ptr<dataformats::Fragment> dataFragPtr(new dataformats::Fragment(&dummy_int, sizeof(dummy_int)));
    dataFragPtr->set_trigger_number(dataReq.trigger_number);
    bool wasSentSuccessfully = false;
    while (!wasSentSuccessfully && running_flag.load()) {
      TLOG(TLVL_WORK_STEPS) << get_name() << ": Pushing the Data Fragment for trigger number "
                            << dataFragPtr->get_trigger_number() << " onto the output queue";
      try {
        dataFragmentOutputQueue_->push(std::move(dataFragPtr), queueTimeout_);
        wasSentSuccessfully = true;
      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
        std::ostringstream oss_warn;
        oss_warn << "push to output queue \"" << dataFragmentOutputQueue_->get_name() << "\"";
        ers::warning(dunedaq::appfwk::QueueTimeoutExpired(
          ERS_HERE,
          get_name(),
          oss_warn.str(),
          std::chrono::duration_cast<std::chrono::milliseconds>(queueTimeout_).count()));
      }
    }

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

DEFINE_DUNE_DAQ_MODULE(dunedaq::ddpdemo::FakeDataProd)
