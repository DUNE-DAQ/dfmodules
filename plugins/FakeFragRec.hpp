/**
 * @file FakeFragRec.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_FAKEFRAGREC_HPP_
#define DFMODULES_PLUGINS_FAKEFRAGREC_HPP_

#include "dataformats/Fragment.hpp"
#include "dataformats/TriggerRecord.hpp"
#include "dfmessages/TriggerDecision.hpp"

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
 * @brief FakeFragRec is simply an example
 */
class FakeFragRec : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief FakeFragRec Constructor
   * @param name Instance name for this FakeFragRec instance
   */
  explicit FakeFragRec(const std::string& name);

  FakeFragRec(const FakeFragRec&) = delete;            ///< FakeFragRec is not copy-constructible
  FakeFragRec& operator=(const FakeFragRec&) = delete; ///< FakeFragRec is not copy-assignable
  FakeFragRec(FakeFragRec&&) = delete;                 ///< FakeFragRec is not move-constructible
  FakeFragRec& operator=(FakeFragRec&&) = delete;      ///< FakeFragRec is not move-assignable

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
  // size_t sleepMsecWhileRunning_;
  std::chrono::milliseconds m_queue_timeout;

  // Queue(s)
  using trigdecsource_t = dunedaq::appfwk::DAQSource<dfmessages::TriggerDecision>;
  std::unique_ptr<trigdecsource_t> m_trigger_decision_input_queue;
  using datafragsource_t = dunedaq::appfwk::DAQSource<std::unique_ptr<dataformats::Fragment>>;
  std::vector<std::unique_ptr<datafragsource_t>> m_data_fragment_input_queues;
  using trigrecsink_t = dunedaq::appfwk::DAQSink<std::unique_ptr<dataformats::TriggerRecord>>;
  std::unique_ptr<trigrecsink_t> m_trigger_record_output_queue;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_FAKEFRAGREC_HPP_
