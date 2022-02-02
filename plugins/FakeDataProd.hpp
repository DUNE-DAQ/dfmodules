/**
 * @file FakeDataProd.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_FAKEDATAPROD_HPP_
#define DFMODULES_PLUGINS_FAKEDATAPROD_HPP_

#include "daqdataformats/Fragment.hpp"
#include "dfmessages/DataRequest.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSink.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dunedaq {

// Disable coverage checking LCOV_EXCL_START
ERS_DECLARE_ISSUE(dfmodules,
                  FragmentTransmissionFailed,
                  mod_name << " failed to send data for trigger number " << tr_num << ".",
                  ((std::string)mod_name)((int64_t)tr_num))

ERS_DECLARE_ISSUE(dfmodules,
                  TimeSyncTransmissionFailed,
                  mod_name << " failed to send send TimeSync message to " << dest << " with topic " << topic << ".",
                  ((std::string)mod_name)((std::string)dest)((std::string)topic))
ERS_DECLARE_ISSUE_BASE(dfmodules,
                       MemoryAllocationFailed,
                       appfwk::GeneralDAQModuleIssue,
                       "Malloc of " << bytes << " bytes failed",
                       ((std::string)name),
                       ((size_t)bytes))

// Disable coverage checking LCOV_EXCL_STOP

namespace dfmodules {

/**
 * @brief FakeDataProd is simply an example
 */
class FakeDataProd : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief FakeDataProd Constructor
   * @param name Instance name for this FakeDataProd instance
   */
  explicit FakeDataProd(const std::string& name);

  FakeDataProd(const FakeDataProd&) = delete;            ///< FakeDataProd is not copy-constructible
  FakeDataProd& operator=(const FakeDataProd&) = delete; ///< FakeDataProd is not copy-assignable
  FakeDataProd(FakeDataProd&&) = delete;                 ///< FakeDataProd is not move-constructible
  FakeDataProd& operator=(FakeDataProd&&) = delete;      ///< FakeDataProd is not move-assignable

  void init(const data_t&) override;

private:
  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);

  void get_info(opmonlib::InfoCollector& ci, int level) override;

  // Threading
  dunedaq::appfwk::ThreadHelper m_thread;
  dunedaq::appfwk::ThreadHelper m_timesync_thread;
  void do_work(std::atomic<bool>&);
  void do_timesync(std::atomic<bool>&);

  // Configuration
  // size_t m_sleep_msec_while_running;
  std::chrono::milliseconds m_queue_timeout;
  dunedaq::daqdataformats::run_number_t m_run_number;
  daqdataformats::GeoID m_geoid;
  uint64_t m_time_tick_diff; // NOLINT (build/unsigned)
  uint64_t m_frame_size;     // NOLINT (build/unsigned)
  uint64_t m_response_delay; // NOLINT (build/unsigned)
  daqdataformats::FragmentType m_fragment_type;
  std::string m_timesync_connection_name;
  std::string m_timesync_topic_name;
  uint32_t m_pid_of_current_process;

  // Queue(s)
  using datareqsource_t = dunedaq::appfwk::DAQSource<dfmessages::DataRequest>;
  std::unique_ptr<datareqsource_t> m_data_request_input_queue;

  std::atomic<uint64_t> m_received_requests{ 0 }; // NOLINT (build/unsigned)
  std::atomic<uint64_t> m_sent_fragments{ 0 };    // NOLINT (build/unsigned)
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_FAKEDATAPROD_HPP_
