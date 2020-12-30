/**
 * @file TriggerDecisionForwarder.hpp TriggerDecisionForwarder Class
 *
 * The TriggerDecisionForwarder class provides functionality to asynchronously
 * forward copies of TriggerDecision messages to an interested listener.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_INCLUDE_DFMODULES_TRIGGERDECISIONFORWARDER_HPP_
#define DFMODULES_INCLUDE_DFMODULES_TRIGGERDECISIONFORWARDER_HPP_

#include "appfwk/DAQSink.hpp"
#include "appfwk/NamedObject.hpp"
#include "appfwk/ThreadHelper.hpp"
#include "dfmessages/TriggerDecision.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief
 */
class TriggerDecisionForwarder : public appfwk::NamedObject
{
public:
  using trigdecsink_t = dunedaq::appfwk::DAQSink<dfmessages::TriggerDecision>;

  /**
   * @brief TriggerDecisionForwarder Constructor
   */
  explicit TriggerDecisionForwarder(const std::string&, std::unique_ptr<trigdecsink_t>);

  TriggerDecisionForwarder(const TriggerDecisionForwarder&) =
    delete; ///< TriggerDecisionForwarder is not copy-constructible
  TriggerDecisionForwarder& operator=(const TriggerDecisionForwarder&) =
    delete;                                                      ///< TriggerDecisionForwarder is not copy-assignable
  TriggerDecisionForwarder(TriggerDecisionForwarder&&) = delete; ///< TriggerDecisionForwarder is not move-constructible
  TriggerDecisionForwarder& operator=(TriggerDecisionForwarder&&) =
    delete; ///< TriggerDecisionForwarder is not move-assignable

  void start_forwarding();

  void stop_forwarding();

  void set_latest_trigger_decision(const dfmessages::TriggerDecision& trig_dec)
  {
    std::lock_guard<std::mutex> lk(data_mutex_);
    latest_trigger_decision_ = trig_dec;
    trig_dec_has_been_sent_ = false;
  }

private:
  // Threading
  dunedaq::appfwk::ThreadHelper thread_;
  void do_work(std::atomic<bool>&);

  // Configuration
  std::chrono::milliseconds queueTimeout_;

  // Queue(s)
  std::unique_ptr<trigdecsink_t> trigger_decision_sink_;

  // Internal data
  std::mutex data_mutex_;
  dfmessages::TriggerDecision latest_trigger_decision_;
  bool trig_dec_has_been_sent_;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_INCLUDE_DFMODULES_TRIGGERDECISIONFORWARDER_HPP_
