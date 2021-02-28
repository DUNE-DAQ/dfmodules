/**
 * @file FakeTrigDecEmu.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_FAKETRIGDECEMU_HPP_
#define DFMODULES_PLUGINS_FAKETRIGDECEMU_HPP_

#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TriggerInhibit.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSink.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief FakeTrigDecEmu is simply an example
 */
class FakeTrigDecEmu : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief FakeTrigDecEmu Constructor
   * @param name Instance name for this FakeTrigDecEmu instance
   */
  explicit FakeTrigDecEmu(const std::string& name);

  FakeTrigDecEmu(const FakeTrigDecEmu&) = delete;            ///< FakeTrigDecEmu is not copy-constructible
  FakeTrigDecEmu& operator=(const FakeTrigDecEmu&) = delete; ///< FakeTrigDecEmu is not copy-assignable
  FakeTrigDecEmu(FakeTrigDecEmu&&) = delete;                 ///< FakeTrigDecEmu is not move-constructible
  FakeTrigDecEmu& operator=(FakeTrigDecEmu&&) = delete;      ///< FakeTrigDecEmu is not move-assignable

  void init(const data_t&) override;

private:
  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);

  // Threading
  dunedaq::appfwk::ThreadHelper m_thread;
  void do_work(std::atomic<bool>&);

  // Configuration
  size_t m_sleep_msec_while_running;
  std::chrono::milliseconds m_queue_timeout;

  // Queue(s)
  using trigdecsink_t = dunedaq::appfwk::DAQSink<dfmessages::TriggerDecision>;
  std::unique_ptr<trigdecsink_t> m_trigger_decision_output_queue;
  using triginhsource_t = dunedaq::appfwk::DAQSource<dfmessages::TriggerInhibit>;
  std::unique_ptr<triginhsource_t> m_trigger_inhibit_input_queue;
  using tokensource_t = dunedaq::appfwk::DAQSource<dfmessages::TriggerDecisionToken>;
  std::unique_ptr<tokensource_t> m_trigger_decision_token_input_queue;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_FAKETRIGDECEMU_HPP_
