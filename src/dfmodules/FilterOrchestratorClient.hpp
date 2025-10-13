/**
 * @file FilterOrchestratorClient.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DFMODULES_FILTERORCHESTRATORCLIENT_HPP_
#define DFMODULES_SRC_DFMODULES_FILTERORCHESTRATORCLIENT_HPP_

#include "logging/Logging.hpp"

#include "nlohmann/json.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/version.hpp>

#include <optional>
#include <string>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>

namespace dunedaq {

    
// Disable coverage collection LCOV_EXCL_START
ERS_DECLARE_ISSUE(dfmodules,
                  RequestFailed,
                  "Orchestrator request to " << target << " failed with message " << result,
                  ((std::string)target)((std::string)result))
// Re-enable coverage collection LCOV_EXCL_STOP

namespace dfmodules {
class FilterOrchestratorClient
{
public:
  struct FilterOrchestratorRecord
  {
    std::string file;
    uint64_t run_number;
    uint64_t trigger_number;
    uint64_t sequence_number;
    std::string type;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FilterOrchestratorRecord, file, run_number, trigger_number, sequence_number, type);
  };
  struct FilterOrchestratorQuery
  {
    std::string host;
    std::string file;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FilterOrchestratorQuery, host, file);
  };

  /**
   * Constructor: Connect to the Filter Orchestrator server
   *
   * @param server  Name/address of the orchestrator server to publish to
   * @param port    Port on the orchestrator server to connect to
   */
  FilterOrchestratorClient(const std::string& server, uint16_t port);

  /**
   * Destructor: stops the publishing hread and retracts all published
   *          information
   */
  ~FilterOrchestratorClient();

  void connect();
  void disconnect();

  void request_tr();
  void request_ts();

  std::optional<FilterOrchestratorRecord> read_next_triggerrecord(std::string const& host,
                                                                  std::string const& file = "");
  std::optional<FilterOrchestratorRecord> read_next_timeslice(std::string const& host, std::string const& file = "");

  void complete(uint64_t run_number,
                uint64_t trigger_number,
                uint64_t sequence_number,
                std::string type = "TriggerRecord");
  void complete(FilterOrchestratorRecord rec);

  bool is_connected() { return m_connected.load(); }

private:
  std::mutex m_mutex;
  net::io_context m_io_context;
  net::ip::basic_resolver<net::ip::tcp>::results_type m_addr;
  boost::beast::tcp_stream m_stream;

  std::atomic<bool> m_connected{ false };
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_DFMODULES_FILTERORCHESTRATORCLIENT_HPP_
