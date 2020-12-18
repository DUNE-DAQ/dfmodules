/**
 * @file FakeTrigDecEmu.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DDPDEMO_SRC_FAKETRIGDECEMU_HPP_
#define DDPDEMO_SRC_FAKETRIGDECEMU_HPP_

#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/TriggerInhibit.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSink.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace ddpdemo {

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
  dunedaq::appfwk::ThreadHelper thread_;
  void do_work(std::atomic<bool>&);

  // Configuration
  size_t sleepMsecWhileRunning_;
  std::chrono::milliseconds queueTimeout_;

  // Queue(s)
  using trigdecsink_t = dunedaq::appfwk::DAQSink<dfmessages::TriggerDecision>;
  std::unique_ptr<trigdecsink_t> triggerDecisionOutputQueue_;
  using triginhsource_t = dunedaq::appfwk::DAQSource<dfmessages::TriggerInhibit>;
  std::unique_ptr<triginhsource_t> triggerInhibitInputQueue_;
};
} // namespace ddpdemo
} // namespace dunedaq

#endif // DDPDEMO_SRC_FAKETRIGDECEMU_HPP_
