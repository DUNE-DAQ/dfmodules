/**
 * @file DataFlowOrchestrator.cpp DataFlowOrchestrator class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DataFlowOrchestrator.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/app/Nljs.hpp"
#include "dfmodules/requestreceiver/Nljs.hpp"
#include "dfmodules/requestreceiver/Structs.hpp"
#include "dfmodules/requestreceiverinfo/InfoNljs.hpp"
#include "logging/Logging.hpp"
#include "networkmanager/NetworkManager.hpp"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "DataFlowOrchestrator" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10
};

namespace dunedaq {
namespace dfmodules {

DataFlowOrchestrator::DataFlowOrchestrator(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_queue_timeout(100)
  , m_run_number(0)
  , m_data_request_output_queues()
{
  register_command("conf", &DataFlowOrchestrator::do_conf);
  register_command("start", &DataFlowOrchestrator::do_start);
  register_command("stop", &DataFlowOrchestrator::do_stop);
  register_command("scrap", &DataFlowOrchestrator::do_scrap);
}

void
DataFlowOrchestrator::init(const data_t& init_data)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  //----------------------
  // Get dynamic queues
  //----------------------

  // set names for the fragment queue(s)
  auto ini = init_data.get<appfwk::app::ModInit>();

  // Test for valid output data request queues
  for (const auto& qitem : ini.qinfos) {
    if (qitem.name.rfind("data_request_") == 0) {
      try {
        datareqsink_t temp(qitem.inst);
      } catch (const ers::Issue& excpt) {
        throw InvalidQueueFatalError(ERS_HERE, get_name(), qitem.name, excpt);
      }
    }
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DataFlowOrchestrator::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  m_data_request_output_queues.clear();

  requestreceiver::ConfParams parsed_conf = payload.get<requestreceiver::ConfParams>();

  for (auto const& entry : parsed_conf.map) {

    daqdataformats::GeoID::SystemType type = daqdataformats::GeoID::string_to_system_type(entry.system);

    if (type == daqdataformats::GeoID::SystemType::kInvalid) {
      throw InvalidSystemType(ERS_HERE, entry.system);
    }

    daqdataformats::GeoID key;
    key.system_type = type;
    key.region_id = entry.region;
    key.element_id = entry.element;
    m_data_request_output_queues[key] = std::unique_ptr<datareqsink_t>(new datareqsink_t(entry.queueinstance));
  }

  m_queue_timeout = std::chrono::milliseconds(parsed_conf.general_queue_timeout);
  m_connection_name = parsed_conf.connection_name;
  TLOG() << "Connection name is " << m_connection_name << std::endl;

  networkmanager::NetworkManager::get().start_listening(m_connection_name);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
DataFlowOrchestrator::do_start(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  m_received_requests = 0;
  m_run_number = payload.value<dunedaq::daqdataformats::run_number_t>("run", 0);

  networkmanager::NetworkManager::get().register_callback(
    m_connection_name, std::bind(&DataFlowOrchestrator::dispatch_request, this, std::placeholders::_1));

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DataFlowOrchestrator::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  networkmanager::NetworkManager::get().clear_callback(m_connection_name);

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DataFlowOrchestrator::do_scrap(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  networkmanager::NetworkManager::get().stop_listening(m_connection_name);

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
DataFlowOrchestrator::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  requestreceiverinfo::Info info;
  info.requests_received = m_received_requests;
  ci.add(info);
}

void
DataFlowOrchestrator::dispatch_request(ipm::Receiver::Response message)
{
  auto request = serialization::deserialize<dfmessages::DataRequest>(message.data);
  TLOG_DEBUG(10) << get_name() << "Received data request: " << request.trigger_number
                 << " Component: " << request.request_information;

  auto component = request.request_information.component;
  if (m_data_request_output_queues.count(component)) {
    TLOG_DEBUG(10) << get_name() << "Dispatch request to queue "
                   << m_data_request_output_queues.at(component)->get_name();
    m_data_request_output_queues.at(component)->push(request, m_queue_timeout);
  } else {
    ers::error(UnknownGeoID(ERS_HERE, component));
  }
  m_received_requests++;
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DataFlowOrchestrator)
