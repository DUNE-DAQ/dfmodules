/**
 * @file DataWriter.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DATAWRITER_HPP_
#define DFMODULES_SRC_DATAWRITER_HPP_

#include "dfmodules/TriggerInhibitAgent.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"
#include "dataformats/TriggerRecord.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief DataWriter is a shell for what we might write for the MiniDAQApp.
 */
class DataWriter : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief DataWriter Constructor
   * @param name Instance name for this DataWriter instance
   */
  explicit DataWriter(const std::string& name);

  DataWriter(const DataWriter&) = delete;            ///< DataWriter is not copy-constructible
  DataWriter& operator=(const DataWriter&) = delete; ///< DataWriter is not copy-assignable
  DataWriter(DataWriter&&) = delete;                 ///< DataWriter is not move-constructible
  DataWriter& operator=(DataWriter&&) = delete;      ///< DataWriter is not move-assignable

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

  // Worker(s)
  std::unique_ptr<TriggerInhibitAgent> trigger_inhibit_agent_;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_DATAWRITER_HPP_
