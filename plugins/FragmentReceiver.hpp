/**
 * @file FragmentReceiver.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_FAKEDATAPROD_HPP_
#define DFMODULES_SRC_FAKEDATAPROD_HPP_

#include "dataformats/Fragment.hpp"
#include "dfmessages/DataRequest.hpp"

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
 * @brief FragmentReceiver is the Module that collects Fragment from the Upstream DAQ Modules, it checks 
 *        if they corresponds to a Trigger Decision, and once a Trigger Decision has all its fragments, 
 *        it sends the data (The complete Trigger Record) to a writer
 */
class FragmentReceiver : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief FragmentReceiver Constructor
   * @param name Instance name for this FragmentReceiver instance
   */
  explicit FragmentReceiver(const std::string& name);

  FragmentReceiver(const FragmentReceiver&) = delete;            ///< FragmentReceiver is not copy-constructible
  FragmentReceiver& operator=(const FragmentReceiver&) = delete; ///< FragmentReceiver is not copy-assignable
  FragmentReceiver(FragmentReceiver&&) = delete;                 ///< FragmentReceiver is not move-constructible
  FragmentReceiver& operator=(FragmentReceiver&&) = delete;      ///< FragmentReceiver is not move-assignable

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
  // using datareqsource_t = dunedaq::appfwk::DAQSource<dfmessages::DataRequest>;
  // std::unique_ptr<datareqsource_t> dataRequestInputQueue_;
  // using datafragsink_t = dunedaq::appfwk::DAQSink<std::unique_ptr<dataformats::Fragment>>;
  // std::unique_ptr<datafragsink_t> dataFragmentOutputQueue_;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_FAKEDATAPROD_HPP_
