/**
 * @file DataFlowOrchestrator.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_
#define DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_

#include "dfmessages/DataRequest.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSink.hpp"
#include "ipm/Receiver.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief DataFlowOrchestrator distributes triggers according to the availability of the DF apps in the system
 */
class DataFlowOrchestrator : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief DataFlowOrchestrator Constructor
   * @param name Instance name for this DataFlowOrchestrator instance
   */
  explicit DataFlowOrchestrator(const std::string& name);

  DataFlowOrchestrator(const DataFlowOrchestrator&) = delete;            ///< DataFlowOrchestrator is not copy-constructible
  DataFlowOrchestrator& operator=(const DataFlowOrchestrator&) = delete; ///< DataFlowOrchestrator is not copy-assignable
  DataFlowOrchestrator(DataFlowOrchestrator&&) = delete;                 ///< DataFlowOrchestrator is not move-constructible
  DataFlowOrchestrator& operator=(DataFlowOrchestrator&&) = delete;      ///< DataFlowOrchestrator is not move-assignable

  void init(const data_t&) override;

private:
  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);
  void do_scrap(const data_t&);

  void get_info(opmonlib::InfoCollector& ci, int level) override;

  void dispatch_request(ipm::Receiver::Response message);

  // Configuration
  std::chrono::milliseconds m_queue_timeout;
  dunedaq::daqdataformats::run_number_t m_run_number;
  std::string m_connection_name;

  // Queue(s)
  using datareqsink_t = dunedaq::appfwk::DAQSink<dfmessages::DataRequest>;
  std::map<daqdataformats::GeoID, std::unique_ptr<datareqsink_t>> m_data_request_output_queues;

  std::atomic<uint64_t> m_received_requests{ 0 }; // NOLINT (build/unsigned)
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_REQUESTRECEIVER_HPP_
