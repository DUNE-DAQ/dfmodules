/**
 * @file FilterOrchestratorClient.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/FilterOrchestratorClient.hpp"

#include "logging/Logging.hpp"

#include <boost/beast/http.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using tcp = net::ip::tcp;     // from <boost/asio/ip/tcp.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
using nlohmann::json;

using namespace dunedaq::dfmodules;

static constexpr int HTTP_V1_1 = 11;

enum
{
  TLVL_REQUEST = 20,
  TLVL_READ = 25,
  TLVL_COMPLETE = 30
};

FilterOrchestratorClient::FilterOrchestratorClient(const std::string& server, uint16_t port)
  : m_stream(m_io_context)
{
  tcp::resolver resolver(m_io_context);
  m_addr = resolver.resolve(server, std::to_string(port));
}

FilterOrchestratorClient::~FilterOrchestratorClient()
{
  disconnect();
}

void
FilterOrchestratorClient::connect()
{
  m_stream.connect(m_addr);
  m_connected = true;
}

void
FilterOrchestratorClient::disconnect()
{
  if (m_connected.load()) {
    m_connected = false;
    beast::error_code ec;
    m_stream.socket().shutdown(tcp::socket::shutdown_both, ec); // NOLINT
  }
}

void
dunedaq::dfmodules::FilterOrchestratorClient::request_tr()
{

  TLOG_DEBUG(TLVL_REQUEST) << "Requesting TriggerRecord to be sent";
  std::lock_guard<std::mutex> lk(m_mutex);
  if (!m_connected.load()) {
    connect();
  }
  std::string target = "/request-tr";
  http::request<http::string_body> req(http::verb::get, target, HTTP_V1_1);

  http::response<http::string_body> response;
  try {
    http::write(m_stream, req);

    boost::beast::flat_buffer buffer;
    http::read(m_stream, buffer, response);

    TLOG_DEBUG(TLVL_REQUEST) << "get " << target << " response: " << response;

    if (response.result_int() != 200) {
      throw(RequestFailed(ERS_HERE, target, std::string(response.reason())));
    }
  } catch (ers::Issue const&) {
    disconnect();
    throw;
  } catch (std::exception const& ex) {
    disconnect();
    ers::error(RequestFailed(ERS_HERE, target, ex.what()));
  }
}

void
dunedaq::dfmodules::FilterOrchestratorClient::request_ts()
{

  TLOG_DEBUG(TLVL_REQUEST) << "Requesting TimeSlice to be sent";
  std::lock_guard<std::mutex> lk(m_mutex);
  if (!m_connected.load()) {
    connect();
  }
  std::string target = "/request-ts";
  http::request<http::string_body> req(http::verb::get, target, HTTP_V1_1);

  http::response<http::string_body> response;
  try {
    http::write(m_stream, req);

    boost::beast::flat_buffer buffer;
    http::read(m_stream, buffer, response);

    TLOG_DEBUG(TLVL_REQUEST) << "get " << target << " response: " << response;

    if (response.result_int() != 200) {
      throw(RequestFailed(ERS_HERE, target, std::string(response.reason())));
    }
  } catch (ers::Issue const&) {
    disconnect();
    throw;
  } catch (std::exception const& ex) {
    disconnect();
    ers::error(RequestFailed(ERS_HERE, target, ex.what()));
  }
}

std::optional<FilterOrchestratorClient::FilterOrchestratorRecord>
FilterOrchestratorClient::read_next_triggerrecord(std::string const& host, std::string const& file)
{
  TLOG_DEBUG(TLVL_READ) << "Getting next TriggerRecord to read from the Orchestrator, if any. Host=\"" << host
                        << "\", file=\"" << file << "\"";
  std::string target = "/read-next-triggerrecord";
  http::request<http::string_body> req{ http::verb::post, target, HTTP_V1_1 };
  req.set(http::field::content_type, "application/json");
  FilterOrchestratorQuery query{ host, file };
  nlohmann::json jquery = query;
  req.body() = jquery.dump();
  req.prepare_payload();

  http::response<http::string_body> response;
  try {
    http::write(m_stream, req);

    boost::beast::flat_buffer buffer;
    http::read(m_stream, buffer, response);

    TLOG_DEBUG(TLVL_READ) << "get " << target << " response: " << response;

    if (response.result_int() == 200) {
      TLOG_DEBUG(TLVL_READ) << "Server responded";
    } else if (response.result_int() == 404) {
      TLOG_DEBUG(TLVL_READ) << "No records found";
      return std::nullopt;
    } else if (response.result_int() == 429) {
      TLOG_DEBUG(TLVL_READ) << "No requests from data filter";
      return std::nullopt;
    } else {
      throw RequestFailed(ERS_HERE, target, std::string(response.reason()));
    }
  } catch (ers::Issue const&) {
    disconnect();
    throw;
  } catch (std::exception const& ex) {
    disconnect();
    ers::error(RequestFailed(ERS_HERE, target, ex.what()));
    return std::nullopt;
  }

  json result = json::parse(response.body());
  TLOG_DEBUG(TLVL_READ) << result.dump();
  auto res = result.get<FilterOrchestratorRecord>();
  return res;
}

std::optional<FilterOrchestratorClient::FilterOrchestratorRecord>
FilterOrchestratorClient::read_next_timeslice(std::string const& host, std::string const& file)
{
  TLOG_DEBUG(TLVL_READ) << "Getting next TimeSlice to read from the Orchestrator, if any. Host=\"" << host
                        << "\", file=\"" << file << "\"";
  std::string target = "/read-next-timeslice";
  http::request<http::string_body> req{ http::verb::post, target, HTTP_V1_1 };
  req.set(http::field::content_type, "application/json");
  FilterOrchestratorQuery query{ host, file };
  nlohmann::json jquery = query;
  req.body() = jquery.dump();
  req.prepare_payload();

  http::response<http::string_body> response;
  try {
    http::write(m_stream, req);

    boost::beast::flat_buffer buffer;
    http::read(m_stream, buffer, response);

    TLOG_DEBUG(TLVL_READ) << "get " << target << " response: " << response;

    if (response.result_int() == 200) {
      TLOG_DEBUG(TLVL_READ) << "Server responded";
    } else if (response.result_int() == 404) {
      TLOG_DEBUG(TLVL_READ) << "No records found";
      return std::nullopt;
    } else if (response.result_int() == 429) {
      TLOG_DEBUG(TLVL_READ) << "No requests from data filter";
      return std::nullopt;
    } else {
      throw RequestFailed(ERS_HERE, target, std::string(response.reason()));
    }
  } catch (ers::Issue const&) {
    disconnect();
    throw;
  } catch (std::exception const& ex) {
    disconnect();
    ers::error(RequestFailed(ERS_HERE, target, ex.what()));
    return std::nullopt;
  }

  json result = json::parse(response.body());
  TLOG_DEBUG(TLVL_READ) << result.dump();
  auto res = result.get<FilterOrchestratorRecord>();
  return res;
}

void
FilterOrchestratorClient::complete(uint64_t run_number,
                                                       uint64_t trigger_number,
                                                       uint64_t sequence_number,
                                                       std::string type)
{
  FilterOrchestratorRecord rec;
  rec.run_number = run_number;
  rec.trigger_number = trigger_number;
  rec.sequence_number = sequence_number;
  rec.type = type;
  complete(rec);
}

void
FilterOrchestratorClient::complete(FilterOrchestratorClient::FilterOrchestratorRecord rec)
{
  TLOG_DEBUG(TLVL_COMPLETE) << "Marking record/slice as complete";
  std::string target = "/complete";
  http::request<http::string_body> req{ http::verb::post, target, HTTP_V1_1 };
  req.set(http::field::content_type, "application/json");
  nlohmann::json jquery = rec;
  req.body() = jquery.dump();
  req.prepare_payload();

  http::response<http::string_body> response;
  try {
    http::write(m_stream, req);

    boost::beast::flat_buffer buffer;
    http::read(m_stream, buffer, response);

    TLOG_DEBUG(TLVL_COMPLETE) << "get " << target << " response: " << response;

    if (response.result_int() == 200) {
      TLOG_DEBUG(TLVL_READ) << "Server responded";
    } else if (response.result_int() == 404) {
      TLOG_DEBUG(TLVL_READ) << "Record not found";
    } else {
      throw RequestFailed(ERS_HERE, target, std::string(response.reason()));
    }
  } catch (ers::Issue const&) {
    disconnect();
    throw;
  } catch (std::exception const& ex) {
    disconnect();
    ers::error(RequestFailed(ERS_HERE, target, ex.what()));
  }
}
