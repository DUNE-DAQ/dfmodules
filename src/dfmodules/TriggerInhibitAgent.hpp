/**
 * @file TriggerInhibitAgent.hpp TriggerInhibitAgent Class
 *
 * The TriggerInhibitAgent class provides functionality to determine
 * if a TriggerInhibit needs to be generated.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DFMODULES_TRIGGERINHIBITAGENT_HPP_
#define DFMODULES_SRC_DFMODULES_TRIGGERINHIBITAGENT_HPP_

#include "appfwk/DAQSink.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/NamedObject.hpp"
#include "appfwk/ThreadHelper.hpp"
#include "dataformats/Types.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/TriggerInhibit.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief
 */
class TriggerInhibitAgent : public appfwk::NamedObject
{
public:
  using trigdecsource_t = dunedaq::appfwk::DAQSource<dfmessages::TriggerDecision>;
  using triginhsink_t = dunedaq::appfwk::DAQSink<dfmessages::TriggerInhibit>;

  /**
   * @brief TriggerInhibitAgent Constructor
   */
  explicit TriggerInhibitAgent(const std::string&, std::unique_ptr<trigdecsource_t>, std::unique_ptr<triginhsink_t>);

  TriggerInhibitAgent(const TriggerInhibitAgent&) = delete; ///< TriggerInhibitAgent is not copy-constructible
  TriggerInhibitAgent& operator=(const TriggerInhibitAgent&) = delete; ///< TriggerInhibitAgent is not copy-assignable
  TriggerInhibitAgent(TriggerInhibitAgent&&) = delete;            ///< TriggerInhibitAgent is not move-constructible
  TriggerInhibitAgent& operator=(TriggerInhibitAgent&&) = delete; ///< TriggerInhibitAgent is not move-assignable

  void start_checking();

  void stop_checking();

  void set_threshold_for_inhibit(uint32_t value) // NOLINT
  {
    threshold_for_inhibit_.store(value);
  }

  void set_latest_trigger_number(dataformats::trigger_number_t trig_num)
  {
    trigger_number_at_end_of_processing_chain_.store(trig_num);
  }

private:
  // Threading
  dunedaq::appfwk::ThreadHelper thread_;
  void do_work(std::atomic<bool>&);

  // Configuration
  std::chrono::milliseconds queueTimeout_;
  std::atomic<uint32_t> threshold_for_inhibit_; // NOLINT

  // Queue(s)
  std::unique_ptr<trigdecsource_t> trigger_decision_source_;
  std::unique_ptr<triginhsink_t> trigger_inhibit_sink_;

  // Internal data
  std::atomic<dataformats::trigger_number_t> trigger_number_at_start_of_processing_chain_;
  std::atomic<dataformats::trigger_number_t> trigger_number_at_end_of_processing_chain_;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_DFMODULES_TRIGGERINHIBITAGENT_HPP_
