/**
 * @file TriggerDecisionForwarder.cpp TriggerDecisionForwarder class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/TriggerDecisionForwarder.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "TRACE/trace.h"
#include "ers/ers.h"

#include <memory>
#include <string>
#include <utility>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "TriggerDecisionForwarder"  // NOLINT
#define TLVL_ENTER_EXIT_METHODS TLVL_DEBUG + 5 // NOLINT
#define TLVL_WORK_STEPS TLVL_DEBUG + 10        // NOLINT

namespace dunedaq {
namespace dfmodules {

TriggerDecisionForwarder::TriggerDecisionForwarder(const std::string& parent_name,
                                                   std::unique_ptr<trigdecsink_t> our_output)
  : NamedObject(parent_name + "::TriggerDecisionForwarder")
  , m_thread(std::bind(&TriggerDecisionForwarder::do_work, this, std::placeholders::_1))
  , m_queue_timeout(100)
  , m_trigger_decision_sink(std::move(our_output))
  , m_trig_dec_has_been_sent(true)
{}

void
TriggerDecisionForwarder::start_forwarding()
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering start_forwarding() method";
  m_thread.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting start_forwarding() method";
}

void
TriggerDecisionForwarder::stop_forwarding()
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering stop_forwarding() method";
  m_thread.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting stop_forwarding() method";
}

void
TriggerDecisionForwarder::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int32_t sent_message_count = 0;

  // 30-Dec-2020, KAB: it would be better to watch a condition variable to see when
  // new TriggerDecisions have been provided (or something like that), rather than
  // using sleeps...

  // work loop
  while (running_flag.load()) {

    // send the latest TriggerDecision, if needed
    std::unique_lock<std::mutex> lk(m_data_mutex);
    if (!m_trig_dec_has_been_sent) {
      TLOG(TLVL_WORK_STEPS) << get_name() << ": Pushing the TriggerDecision for trigger number "
                            << m_latest_trigger_decision.m_trigger_number << " onto the output queue.";
      try {
        m_trigger_decision_sink->push(m_latest_trigger_decision, m_queue_timeout / 2);
        m_trig_dec_has_been_sent = true;
        ++sent_message_count;
      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
        // It is not ideal if we fail to send the TriggerDecision message out, but rather than
        // retrying some unknown number of times, we simply output a TRACE message and
        // go on.  This has the benefit of being responsive to updates to the latest TriggerDecision.
        TLOG(TLVL_WORK_STEPS) << get_name() << ": TIMEOUT pushing a TriggerDecision message onto the output queue";
      }

      // this sleep is intended to allow updates to the latest TriggerDecision to happen in parallel
      lk.unlock();
      std::this_thread::sleep_for(std::chrono::milliseconds(m_queue_timeout / 2));
    } else {
      lk.unlock();
      std::this_thread::sleep_for(std::chrono::milliseconds(m_queue_timeout));
    }
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, sent " << sent_message_count << " TriggerDecision messages.";
  ers::log(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace dfmodules
} // namespace dunedaq
