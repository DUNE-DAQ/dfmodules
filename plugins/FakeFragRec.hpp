/**
 * @file FakeFragRec.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DDPDEMO_SRC_FAKEFRAGREC_HPP_
#define DDPDEMO_SRC_FAKEFRAGREC_HPP_

#include "dataformats/Fragment.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dataformats/TriggerRecord.hpp"

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
  dunedaq::appfwk::ThreadHelper thread_;
  void do_work(std::atomic<bool>&);

  // Configuration
  // size_t sleepMsecWhileRunning_;
  std::chrono::milliseconds queueTimeout_;

  // Queue(s)
  using trigdecsource_t = dunedaq::appfwk::DAQSource<dfmessages::TriggerDecision>;
  std::unique_ptr<trigdecsource_t> triggerDecisionInputQueue_;
  using datafragsource_t = dunedaq::appfwk::DAQSource<std::unique_ptr<dataformats::Fragment>>;
  std::vector<std::unique_ptr<datafragsource_t>> dataFragmentInputQueues_;
  using trigrecsink_t = dunedaq::appfwk::DAQSink<std::unique_ptr<dataformats::TriggerRecord>>;
  std::unique_ptr<trigrecsink_t> triggerRecordOutputQueue_;
};
} // namespace ddpdemo
} // namespace dunedaq

#endif // DDPDEMO_SRC_FAKEFRAGREC_HPP_
