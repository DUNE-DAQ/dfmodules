/**
 * @file FakeTrigDecEmu.cpp FakeTrigDecEmu class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FakeTrigDecEmu.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/faketrigdecemu/Nljs.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "logging/Logging.hpp"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "FakeTrigDecEmu" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_WORK_STEPS = 10
};

namespace dunedaq {
namespace dfmodules {

FakeTrigDecEmu::FakeTrigDecEmu(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_thread(std::bind(&FakeTrigDecEmu::do_work, this, std::placeholders::_1))
  , m_queue_timeout(100)
  , m_trigger_decision_output_queue(nullptr)
  , m_trigger_inhibit_input_queue(nullptr)
  , m_trigger_decision_token_input_queue(nullptr)
{
  register_command("conf", &FakeTrigDecEmu::do_conf);
  register_command("start", &FakeTrigDecEmu::do_start);
  register_command("stop", &FakeTrigDecEmu::do_stop);
}

void
FakeTrigDecEmu::init(const data_t& init_data)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto qi = appfwk::queue_index(init_data, { "trigger_decision_sink", "trigger_inhibit_source" });
  try {
    m_trigger_decision_output_queue.reset(new trigdecsink_t(qi["trigger_decision_sink"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_sink", excpt);
  }
  try {
    m_trigger_inhibit_input_queue.reset(new triginhsource_t(qi["trigger_inhibit_source"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_inhibit_source", excpt);
  }
  try {
    m_trigger_decision_token_input_queue.reset(new tokensource_t(qi["buffer_token_source"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "buffer_token_source", excpt);
  }
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
FakeTrigDecEmu::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  faketrigdecemu::Conf tmpConfig = payload.get<faketrigdecemu::Conf>();
  m_sleep_msec_while_running = tmpConfig.sleep_msec_while_running;

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
FakeTrigDecEmu::do_start(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  m_thread.start_working_thread();
  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
FakeTrigDecEmu::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  m_thread.stop_working_thread();
  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
FakeTrigDecEmu::do_work(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int32_t triggerCount = 0;
  int32_t inhibit_message_count = 0;
  int32_t token_count = 0;

  while (running_flag.load()) {
    auto start_time = std::chrono::steady_clock::now();
    ++triggerCount;
    dfmessages::TriggerDecision trigDecision;
    trigDecision.m_trigger_number = triggerCount;
    trigDecision.m_trigger_timestamp = 0x123456789abcdef0; // placeholder

    bool wasSentSuccessfully = false;
    while (!wasSentSuccessfully && running_flag.load()) {
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Pushing the TriggerDecision for trigger number "
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

    if (m_trigger_inhibit_input_queue != nullptr) {
    
    // receive any/all Inhibit messages that are waiting
    // (This really needs to be done in a separate thread, but we'll trust the
    // official TriggerDecisionEmulator to take care of things like that.)
    bool got_inh_msg = true;
    while (got_inh_msg) {
      try {
        dfmessages::TriggerInhibit trig_inhibit_msg;
        m_trigger_inhibit_input_queue->pop(trig_inhibit_msg, (m_queue_timeout / 10));
        ++inhibit_message_count;
        got_inh_msg = true;
        TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Popped a TriggerInhibit message with busy state set to \""
                                    << trig_inhibit_msg.m_busy << "\" off the inhibit input queue";

        // for now, we just throw these on the floor...
      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
        // it is perfectly reasonable that there might be no data in the queue
        // some fraction of the times that we check, so we just continue
        got_inh_msg = false;
      }
    }
    }

    if (m_trigger_decision_token_input_queue != nullptr) {

      // receive any/all Token messages that are waiting
      // (This really needs to be done in a separate thread, but we'll trust the
      // official TriggerDecisionEmulator to take care of things like that.)

      while (m_trigger_decision_token_input_queue->can_pop()) {
          dfmessages::TriggerDecisionToken td_token_msg;
        m_trigger_decision_token_input_queue->pop(td_token_msg);
          token_count++;
          TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Popped a TriggerDecisionToken message with run number \""
                                << td_token_msg.run_number << "\" off the token input queue";

      }
    }

    auto time_to_wait =
      (start_time + std::chrono::milliseconds(m_sleep_msec_while_running)) - std::chrono::steady_clock::now();
    TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep between fake triggers";
    std::this_thread::sleep_for(time_to_wait);
    TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": End of sleep between fake triggers";
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, generated " << triggerCount << " Fake TriggerDecision messages "
           << "and received " << token_count << " TriggerDecisionToken messages and " << inhibit_message_count << " TriggerInhbit messages of all types (both Busy and Free).";
  TLOG() << ProgressUpdate(ERS_HERE, get_name(), oss_summ.str());
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";

}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::FakeTrigDecEmu)
