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

#ifndef DFMODULES_SRC_DFMODULES_TRIGGERDECISIONFORWARDER_HPP_
#define DFMODULES_SRC_DFMODULES_TRIGGERDECISIONFORWARDER_HPP_

#include "iomanager/Sender.hpp"
#include "utilities/NamedObject.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "utilities/WorkerThread.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief
 */
class TriggerDecisionForwarder : public utilities::NamedObject
{
public:
  using trigdecsender_t = iomanager::SenderConcept<dfmessages::TriggerDecision>;

  /**
   * @brief TriggerDecisionForwarder Constructor
   */
  explicit TriggerDecisionForwarder(const std::string&, std::shared_ptr<trigdecsender_t>);

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
    std::lock_guard<std::mutex> lk(m_data_mutex);
    m_latest_trigger_decision = trig_dec;
    m_trig_dec_has_been_sent = false;
  }

private:
  // Threading
  dunedaq::utilities::WorkerThread m_thread;
  void do_work(std::atomic<bool>&);

  // Configuration
  std::chrono::milliseconds m_queue_timeout;

  // Connections
  std::shared_ptr<trigdecsender_t> m_trigger_decision_sender;

  // Internal data
  std::mutex m_data_mutex;
  dfmessages::TriggerDecision m_latest_trigger_decision;
  bool m_trig_dec_has_been_sent;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_DFMODULES_TRIGGERDECISIONFORWARDER_HPP_
