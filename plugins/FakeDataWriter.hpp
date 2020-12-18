/**
 * @file FakeDataWriter.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DDPDEMO_SRC_FAKEDATAWRITER_HPP_
#define DDPDEMO_SRC_FAKEDATAWRITER_HPP_

#include "dataformats/TriggerRecord.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace ddpdemo {

/**
 * @brief FakeDataWriter is simply an example
 */
class FakeDataWriter : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief FakeDataWriter Constructor
   * @param name Instance name for this FakeDataWriter instance
   */
  explicit FakeDataWriter(const std::string& name);

  FakeDataWriter(const FakeDataWriter&) = delete;            ///< FakeDataWriter is not copy-constructible
  FakeDataWriter& operator=(const FakeDataWriter&) = delete; ///< FakeDataWriter is not copy-assignable
  FakeDataWriter(FakeDataWriter&&) = delete;                 ///< FakeDataWriter is not move-constructible
  FakeDataWriter& operator=(FakeDataWriter&&) = delete;      ///< FakeDataWriter is not move-assignable

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
  using trigrecsource_t = dunedaq::appfwk::DAQSource<std::unique_ptr<dataformats::TriggerRecord>>;
  std::unique_ptr<trigrecsource_t> triggerRecordInputQueue_;
};
} // namespace ddpdemo
} // namespace dunedaq

#endif // DDPDEMO_SRC_FAKEDATAWRITER_HPP_
