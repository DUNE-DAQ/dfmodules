/**
 * @file DispatcherModule.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_DISPATCHERMODULE_HPP_
#define DFMODULES_PLUGINS_DISPATCHERMODULE_HPP_

#include "dfmodules/HDF5DataReader.hpp"
#include "dfmodules/FilterOrchestratorClient.hpp"

#include "daqdataformats/TriggerRecord.hpp"
#include "daqdataformats/TimeSlice.hpp"

#include "appfwk/DAQModule.hpp"
#include "appmodel/DispatcherConf.hpp"
#include "logging/Logging.hpp" // NOTE: if ISSUES ARE DECLARED BEFORE include logging/Logging.hpp, TLOG_DEBUG<<issue wont work.
#include "utilities/WorkerThread.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dunedaq {


namespace dfmodules {

/**
 * @brief DispatcherModule is simply an example
 */
class DispatcherModule : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief DispatcherModule Constructor
   * @param name Instance name for this DispatcherModule instance
   */
  explicit DispatcherModule(const std::string& name);

  DispatcherModule(const DispatcherModule&) = delete;            ///< DispatcherModule is not copy-constructible
  DispatcherModule& operator=(const DispatcherModule&) = delete; ///< DispatcherModule is not copy-assignable
  DispatcherModule(DispatcherModule&&) = delete;                 ///< DispatcherModule is not move-constructible
  DispatcherModule& operator=(DispatcherModule&&) = delete;      ///< DispatcherModule is not move-assignable

  void init(std::shared_ptr<appfwk::ConfigurationManager> mcfg) override;

private:
  // Commands
  void do_conf(const CommandData_t&);
  void do_start(const CommandData_t&);
  void do_stop(const CommandData_t&);

  //  void get_info(opmonlib::InfoCollector& ci, int level) override;

  // Threading
  dunedaq::utilities::WorkerThread m_tr_thread;
  dunedaq::utilities::WorkerThread m_ts_thread;
  void read_trigger_records(std::atomic<bool>&);
  void read_time_slices(std::atomic<bool>&);

  // Configuration
  const appmodel::DispatcherConf* m_dispatcher_conf;
  std::chrono::milliseconds m_request_interval;
  std::chrono::milliseconds m_queue_timeout;
  
  std::string m_host;

  std::shared_ptr<iomanager::SenderConcept<daqdataformats::TriggerRecord>> m_trigger_record_queue;
  std::shared_ptr<iomanager::SenderConcept<daqdataformats::TimeSlice>> m_time_slice_queue;

  std::shared_ptr<HDF5DataReader> m_reader;
  std::shared_ptr<FilterOrchestratorClient> m_client;

  std::atomic<size_t> m_sent_trigger_records{ 0 };
  std::atomic<size_t> m_sent_time_slices{ 0 };
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_DISPATCHERMODULE_HPP_
