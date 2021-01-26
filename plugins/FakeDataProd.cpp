/**
 * @file FakeDataProd.cpp FakeDataProd class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FakeDataProd.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/fakedataprod/Nljs.hpp"

#include "appfwk/DAQModuleHelper.hpp"

#include "TRACE/trace.h"
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
#define TRACE_NAME "FakeDataProd"              // NOLINT
#define TLVL_ENTER_EXIT_METHODS TLVL_DEBUG + 5 // NOLINT
#define TLVL_CONFIG TLVL_DEBUG + 7             // NOLINT
#define TLVL_WORK_STEPS TLVL_DEBUG + 10        // NOLINT

namespace dunedaq {
namespace dfmodules {

FakeDataProd::FakeDataProd(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_thread(std::bind(&FakeDataProd::do_work, this, std::placeholders::_1))
  , m_queue_timeout(100)
  , m_run_number(0)
  , m_fake_link_number(0)
  , m_data_request_input_queue(nullptr)
  , m_data_fragment_output_queue(nullptr)
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
    m_data_request_input_queue.reset(new datareqsource_t(qi["data_request_input_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "data_request_input_queue", excpt);
  }
  try {
    m_data_fragment_output_queue.reset(new datafragsink_t(qi["data_fragment_output_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "data_fragment_output_queue", excpt);
  }
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
FakeDataProd::do_conf(const data_t& payload)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  fakedataprod::ConfParams tmpConfig = payload.get<fakedataprod::ConfParams>();
  m_fake_link_number = tmpConfig.temporarily_hacked_link_number;
  TLOG(TLVL_CONFIG) << get_name() << ": configured for link number " << m_fake_link_number;

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
FakeDataProd::do_start(const data_t& payload)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  m_run_number = payload.value<dunedaq::dataformats::run_number_t>("run", 0);
  m_thread.start_working_thread();
  ERS_LOG(get_name() << " successfully started for run number " << m_run_number);
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
FakeDataProd::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  m_thread.stop_working_thread();
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
      m_data_request_input_queue->pop(dataReq, m_queue_timeout);
      ++receivedCount;
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      // it is perfectly reasonable that there might be no data in the queue
      // some fraction of the times that we check, so we just continue on and try again
      continue;
    }

    // NOLINT TODO PAR 2020-12-17: dataformats::Fragment has to be
    // constructed with some payload data, so I'm putting a few ints
    // in it for now
    int dummy_ints[3];
    dummy_ints[0] = 3;
    dummy_ints[1] = 4;
    dummy_ints[2] = 5;
    std::unique_ptr<dataformats::Fragment> dataFragPtr(new dataformats::Fragment(&dummy_ints[0], sizeof(dummy_ints)));
    dataFragPtr->set_trigger_number(dataReq.trigger_number);
    dataFragPtr->set_run_number(m_run_number);
    dunedaq::dataformats::GeoID geo_location;
    geo_location.apa_number = 1;
    geo_location.link_number = m_fake_link_number;
    dataFragPtr->set_link_id(geo_location);
    dataFragPtr->set_error_bits(0);
    dataFragPtr->set_type(0x123); // placeholder
    dataFragPtr->set_trigger_timestamp(dataReq.trigger_timestamp);
    dataFragPtr->set_window_offset(dataReq.window_offset);
    dataFragPtr->set_window_width(dataReq.window_width);

    // to-do?  add config parameter for artificial delay?
    // if ((dataReq.trigger_number % 7) == 0) {
    //  std::this_thread::sleep_for(std::chrono::milliseconds(4550));
    //}

    bool wasSentSuccessfully = false;
    while (!wasSentSuccessfully && running_flag.load()) {
      TLOG(TLVL_WORK_STEPS) << get_name() << ": Pushing the Data Fragment for trigger number "
                            << dataFragPtr->get_trigger_number() << " onto the output queue";
      try {
        m_data_fragment_output_queue->push(std::move(dataFragPtr), m_queue_timeout);
        wasSentSuccessfully = true;
      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
        std::ostringstream oss_warn;
        oss_warn << "push to output queue \"" << m_data_fragment_output_queue->get_name() << "\"";
        ers::warning(dunedaq::appfwk::QueueTimeoutExpired(
          ERS_HERE,
          get_name(),
          oss_warn.str(),
          std::chrono::duration_cast<std::chrono::milliseconds>(m_queue_timeout).count()));
      }
    }

    // TLOG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep while waiting for run Stop";
    // std::this_thread::sleep_for(std::chrono::milliseconds(sleepMsecWhileRunning_));
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

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::FakeDataProd)
