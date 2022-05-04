/**
 * @file TriggerDecisionForwarder.cpp TriggerDecisionForwarder class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/TriggerDecisionForwarder.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "logging/Logging.hpp"

#include <memory>
#include <string>
#include <utility>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "TriggerDecisionForwarder" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_WORK_STEPS = 10
};

namespace dunedaq {
namespace dfmodules {

TriggerDecisionForwarder::TriggerDecisionForwarder(const std::string& parent_name,
                                                   std::shared_ptr<trigdecsender_t> our_output)
  : NamedObject(parent_name + "::TriggerDecisionForwarder")
  , m_thread(std::bind(&TriggerDecisionForwarder::do_work, this, std::placeholders::_1))
  , m_queue_timeout(100)
  , m_trigger_decision_sender(our_output)
  , m_trig_dec_has_been_sent(true)
{}

void
TriggerDecisionForwarder::start_forwarding()
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering start_forwarding() method";
  m_thread.start_working_thread();
  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting start_forwarding() method";
}

void
TriggerDecisionForwarder::stop_forwarding()
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering stop_forwarding() method";
  m_thread.stop_working_thread();
  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting stop_forwarding() method";
}

void
TriggerDecisionForwarder::do_work(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int32_t sent_message_count = 0;

  // 30-Dec-2020, KAB: it would be better to watch a condition variable to see when
  // new TriggerDecisions have been provided (or something like that), rather than
  // using sleeps...

  // work loop
  while (running_flag.load()) {

    // send the latest TriggerDecision, if needed
    std::unique_lock<std::mutex> lk(m_data_mutex);
    if (!m_trig_dec_has_been_sent) {
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Pushing the TriggerDecision for trigger number "
                                  << m_latest_trigger_decision.trigger_number << " onto the output queue.";
      try {
        m_trigger_decision_sender->send(m_latest_trigger_decision, m_queue_timeout / 2);
        m_trig_dec_has_been_sent = true;
        ++sent_message_count;
      } catch (const iomanager::TimeoutExpired& excpt) {
        // It is not ideal if we fail to send the TriggerDecision message out, but rather than
        // retrying some unknown number of times, we simply output a TRACE message and
        // go on.  This has the benefit of being responsive to updates to the latest TriggerDecision.
        TLOG_DEBUG(TLVL_WORK_STEPS) << get_name()
                                    << ": TIMEOUT pushing a TriggerDecision message onto the output connection";
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
  TLOG() << ProgressUpdate(ERS_HERE, get_name(), oss_summ.str());
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace dfmodules
} // namespace dunedaq
