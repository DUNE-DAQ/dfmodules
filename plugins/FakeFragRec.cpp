/**
 * @file FakeFragRec.cpp FakeFragRec class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FakeFragRec.hpp"
#include "dfmodules/CommonIssues.hpp"
//#include "dfmodules/fakefragrec/Nljs.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/cmd/Nljs.hpp"
//#include "TRACE/trace.h"
//#include "ers/ers.h"
#include "logging/Logging.hpp"

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
#define TRACE_NAME "FakeFragRec"               // NOLINT
#define TLVL_ENTER_EXIT_METHODS 5			   // NOLINT
#define TLVL_WORK_STEPS 10					   // NOLINT

namespace dunedaq {
namespace dfmodules {

FakeFragRec::FakeFragRec(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_thread(std::bind(&FakeFragRec::do_work, this, std::placeholders::_1))
  , m_queue_timeout(100)
  , m_trigger_decision_input_queue(nullptr)
  , m_data_fragment_input_queues()
  , m_trigger_record_output_queue(nullptr)
{
  register_command("conf", &FakeFragRec::do_conf);
  register_command("start", &FakeFragRec::do_start);
  register_command("stop", &FakeFragRec::do_stop);
}

void
FakeFragRec::init(const data_t& init_data)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto qilist = appfwk::queue_index(init_data, { "trigger_decision_input_queue", "trigger_record_output_queue" });
  try {
    m_trigger_decision_input_queue.reset(new trigdecsource_t(qilist["trigger_decision_input_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_input_queue", excpt);
  }
  try {
    m_trigger_record_output_queue.reset(new trigrecsink_t(qilist["trigger_record_output_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_record_output_queue", excpt);
  }
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";

  auto ini = init_data.get<appfwk::cmd::ModInit>();
  for (const auto& qitem : ini.qinfos) {
    if (qitem.name.rfind("data_fragment_") == 0) {
      try {
        m_data_fragment_input_queues.emplace_back(new datafragsource_t(qitem.inst));
      } catch (const ers::Issue& excpt) {
        throw InvalidQueueFatalError(ERS_HERE, get_name(), qitem.name, excpt);
      }
    }
  }
}

void
FakeFragRec::do_conf(const data_t& /*payload*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  // fakefragrec::Conf tmpConfig = payload.get<fakefragrec::Conf>();
  // m_sleep_msec_while_running = tmpConfig.sleep_msec_while_running;

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
FakeFragRec::do_start(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  m_thread.start_working_thread();
  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
FakeFragRec::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  m_thread.stop_working_thread();
  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
FakeFragRec::do_work(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int32_t receivedTriggerCount = 0;
  int32_t receivedFragmentCount = 0;

  while (running_flag.load()) {
    dfmessages::TriggerDecision trigDecision;
    try {
      m_trigger_decision_input_queue->pop(trigDecision, m_queue_timeout);
      ++receivedTriggerCount;
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Popped the TriggerDecision for trigger number "
                            << trigDecision.trigger_number << " off the input queue";
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      // it is perfectly reasonable that there might be no data in the queue
      // some fraction of the times that we check, so we just continue on and try again
      continue;
    }

    // 19-Dec-2020, KAB: implemented a simple-minded approach to the Fragments.
    // We'll say that once we receive a TriggerDecision message, we'll wait for one
    // fragment from each of the Fragment Producers, and add those Fragments to the
    // a TriggerRecord that we create.  This is certainly too simple-minded for any
    // real implementation, but this is just a Fake...

    std::vector<std::unique_ptr<dataformats::Fragment>> frag_ptr_vector;
    for (auto& dataFragQueue : m_data_fragment_input_queues) {
      bool got_fragment = false;
      while (!got_fragment && running_flag.load()) {
        try {
          std::unique_ptr<dataformats::Fragment> data_fragment_ptr;
          dataFragQueue->pop(data_fragment_ptr, m_queue_timeout);
          got_fragment = true;
          ++receivedFragmentCount;
          frag_ptr_vector.emplace_back(std::move(data_fragment_ptr));
        } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
          // simply try again (forever); this is clearly a bad idea...
        }
      }
    }

    std::unique_ptr<dataformats::TriggerRecord> trig_rec_ptr(new dataformats::TriggerRecord());
    trig_rec_ptr->set_trigger_number(trigDecision.trigger_number);
    trig_rec_ptr->set_run_number(trigDecision.run_number);
    trig_rec_ptr->set_trigger_timestamp(trigDecision.trigger_timestamp);
    trig_rec_ptr->set_fragments(std::move(frag_ptr_vector));

    bool was_sent_successfully = false;
    while (!was_sent_successfully && running_flag.load()) {
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Pushing the Trigger Record for trigger number "
                            << trig_rec_ptr->get_trigger_number() << " onto the output queue";
      try {
        m_trigger_record_output_queue->push(std::move(trig_rec_ptr), m_queue_timeout);
        was_sent_successfully = true;
      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
        std::ostringstream oss_warn;
        oss_warn << "push to output queue \"" << m_trigger_record_output_queue->get_name() << "\"";
        ers::warning(dunedaq::appfwk::QueueTimeoutExpired(
          ERS_HERE,
          get_name(),
          oss_warn.str(),
          std::chrono::duration_cast<std::chrono::milliseconds>(m_queue_timeout).count()));
      }
    }

    // TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep while waiting for run Stop";
    // std::this_thread::sleep_for(std::chrono::milliseconds(m_sleep_msec_while_running));
    // TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": End of sleep while waiting for run Stop";
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, received " << receivedTriggerCount
           << " Fake trigger decision messages and " << receivedFragmentCount << " Fake data fragmentss.";
  ers::log(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::FakeFragRec)
