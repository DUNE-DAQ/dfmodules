/**
 * @file FakeReqGen.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_FAKEREQGEN_HPP_
#define DFMODULES_PLUGINS_FAKEREQGEN_HPP_

#include "dfmodules/TriggerDecisionForwarder.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSink.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"
#include "dfmessages/DataRequest.hpp"
#include "dfmessages/TriggerDecision.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief FakeReqGen is simply an example
 */
class FakeReqGen : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief FakeReqGen Constructor
   * @param name Instance name for this FakeReqGen instance
   */
  explicit FakeReqGen(const std::string& name);

  FakeReqGen(const FakeReqGen&) = delete;            ///< FakeReqGen is not copy-constructible
  FakeReqGen& operator=(const FakeReqGen&) = delete; ///< FakeReqGen is not copy-assignable
  FakeReqGen(FakeReqGen&&) = delete;                 ///< FakeReqGen is not move-constructible
  FakeReqGen& operator=(FakeReqGen&&) = delete;      ///< FakeReqGen is not move-assignable

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
  // size_t m_sleep_msec_while_running;
  std::chrono::milliseconds m_queue_timeout;

  // Queue(s)
  using trigdecsource_t = dunedaq::appfwk::DAQSource<dfmessages::TriggerDecision>;
  std::unique_ptr<trigdecsource_t> m_trigger_decision_input_queue;
  using trigdecsink_t = dunedaq::appfwk::DAQSink<dfmessages::TriggerDecision>;
  std::unique_ptr<trigdecsink_t> m_trigger_decision_output_queue;
  using datareqsink_t = dunedaq::appfwk::DAQSink<dfmessages::DataRequest>;
  std::vector<std::unique_ptr<datareqsink_t>> m_data_request_output_queues;

  // Worker(s)
  std::unique_ptr<TriggerDecisionForwarder> m_trigger_decision_forwarder;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_FAKEREQGEN_HPP_
