/**
 * @file RequestReceiver.cpp RequestReceiver class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "RequestReceiver.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/app/Nljs.hpp"
#include "dfmodules/requestreceiver/Nljs.hpp"
#include "dfmodules/requestreceiver/Structs.hpp"
#include "dfmodules/requestreceiverinfo/InfoNljs.hpp"
#include "iomanager/IOManager.hpp"
#include "logging/Logging.hpp"

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
#define TRACE_NAME "RequestReceiver" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10
};

namespace dunedaq {
namespace dfmodules {

RequestReceiver::RequestReceiver(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_queue_timeout(100)
  , m_run_number(0)
  , m_data_request_outputs()
{
  register_command("conf", &RequestReceiver::do_conf);
  register_command("start", &RequestReceiver::do_start);
  register_command("stop", &RequestReceiver::do_stop);
  register_command("scrap", &RequestReceiver::do_scrap);
}

void
RequestReceiver::init(const data_t& init_data)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  //----------------------
  // Get dynamic queues
  //----------------------

  // set names for the fragment queue(s)
  auto ini = init_data.get<appfwk::app::ModInit>();

  // Test for valid output data request queues
  auto iom = iomanager::IOManager::get();
  for (const auto& ref : ini.conn_refs) {
    if (ref.name.find("input") == std::string::npos) {
      iom->get_sender<incoming_t>(ref.uid);
    } else {
      iom->get_receiver<incoming_t>(ref.uid);
      m_incoming_data_id = ref.uid;
    }
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
RequestReceiver::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  m_data_request_outputs.clear();

  requestreceiver::ConfParams parsed_conf = payload.get<requestreceiver::ConfParams>();
  auto iom = iomanager::IOManager::get();
  for (auto const& entry : parsed_conf.map) {

    daqdataformats::SourceID::Subsystem type = daqdataformats::SourceID::string_to_subsystem(entry.system);

    if (type == daqdataformats::SourceID::Subsystem::kUnknown) {
      throw InvalidSystemType(ERS_HERE, entry.system);
    }

    daqdataformats::SourceID key;
    key.subsystem = type;
    key.id = entry.source_id;
    m_data_request_outputs[key] = iom->get_sender<incoming_t>(entry.connection_uid);
  }

  m_queue_timeout = std::chrono::milliseconds(parsed_conf.general_queue_timeout);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
RequestReceiver::do_start(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  m_received_requests = 0;
  m_run_number = payload.value<dunedaq::daqdataformats::run_number_t>("run", 0);

  auto iom = iomanager::IOManager::get();
  iom->add_callback<incoming_t>(m_incoming_data_id,
                                std::bind(&RequestReceiver::dispatch_request, this, std::placeholders::_1));

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
RequestReceiver::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  auto iom = iomanager::IOManager::get();
  iom->remove_callback<incoming_t>(m_incoming_data_id);

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
RequestReceiver::do_scrap(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
RequestReceiver::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  requestreceiverinfo::Info info;
  info.requests_received = m_received_requests;
  ci.add(info);
}

void
RequestReceiver::dispatch_request(incoming_t& request)
{
  TLOG_DEBUG(10) << get_name() << "Received data request: " << request.trigger_number
                 << " Component: " << request.request_information;

  auto component = request.request_information.component;
  auto it = m_data_request_outputs.find(component);
  if (it != m_data_request_outputs.end()) {
    TLOG_DEBUG(10) << get_name() << "Dispatch request to queue " << it->second->get_name();
    it->second->send(std::move(request), m_queue_timeout);
  } else {
    ers::error(UnknownSourceID(ERS_HERE, component));
  }
  m_received_requests++;
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::RequestReceiver)
