/**
 * @file RequestReceiver.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_REQUESTRECEIVER_HPP_
#define DFMODULES_PLUGINS_REQUESTRECEIVER_HPP_

#include "dfmessages/DataRequest.hpp"

#include "appfwk/DAQModule.hpp"
#include "iomanager/Sender.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief RequestReceiver receives requests then dispatches them to the appropriate queue
 */
class RequestReceiver : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief RequestReceiver Constructor
   * @param name Instance name for this RequestReceiver instance
   */
  explicit RequestReceiver(const std::string& name);

  RequestReceiver(const RequestReceiver&) = delete;            ///< RequestReceiver is not copy-constructible
  RequestReceiver& operator=(const RequestReceiver&) = delete; ///< RequestReceiver is not copy-assignable
  RequestReceiver(RequestReceiver&&) = delete;                 ///< RequestReceiver is not move-constructible
  RequestReceiver& operator=(RequestReceiver&&) = delete;      ///< RequestReceiver is not move-assignable

  void init(const data_t&) override;

private:
  // type defionition
  using incoming_t = dfmessages::DataRequest;

  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);
  void do_scrap(const data_t&);

  void get_info(opmonlib::InfoCollector& ci, int level) override;

  void dispatch_request(incoming_t&);

  // Configuration
  std::chrono::milliseconds m_queue_timeout;
  dunedaq::daqdataformats::run_number_t m_run_number;

  // Connections
  iomanager::connection::ConnectionRef m_incoming_data_ref;
  using datareqsender_t = dunedaq::iomanager::SenderConcept<incoming_t>;
  std::map<daqdataformats::GeoID, std::shared_ptr<datareqsender_t>> m_data_request_outputs;

  std::atomic<uint64_t> m_received_requests{ 0 }; // NOLINT (build/unsigned)
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_REQUESTRECEIVER_HPP_
