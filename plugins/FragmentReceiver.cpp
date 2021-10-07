/**
 * @file FragmentReceiver.cpp FragmentReceiver class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FragmentReceiver.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/app/Nljs.hpp"
#include "dfmessages/Fragment_serialization.hpp"
#include "dfmodules/fragmentreceiver/Nljs.hpp"
#include "dfmodules/fragmentreceiver/Structs.hpp"
#include "dfmodules/fragmentreceiverinfo/InfoNljs.hpp"
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
#define TRACE_NAME "FragmentReceiver" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10
};

namespace dunedaq {
namespace dfmodules {

FragmentReceiver::FragmentReceiver(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_queue_timeout(100)
  , m_run_number(0)
  , m_fragment_output_queues()
{
  register_command("conf", &FragmentReceiver::do_conf);
  register_command("start", &FragmentReceiver::do_start);
  register_command("stop", &FragmentReceiver::do_stop);
}

void
FragmentReceiver::init(const data_t& init_data)
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
        fragmentsink_t temp(qitem.inst);
      } catch (const ers::Issue& excpt) {
        throw InvalidQueueFatalError(ERS_HERE, get_name(), qitem.name, excpt);
      }
    }
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
FragmentReceiver::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  m_fragment_output_queues.clear();

  fragmentreceiver::ConfParams parsed_conf = payload.get<fragmentreceiver::ConfParams>();

  for (auto const& entry : parsed_conf.map) {

    dataformats::GeoID::SystemType type = dataformats::GeoID::string_to_system_type(entry.system);

    if (type == dataformats::GeoID::SystemType::kInvalid) {
      throw InvalidSystemType(ERS_HERE, entry.system);
    }

    dataformats::GeoID key;
    key.system_type = type;
    key.region_id = entry.region;
    key.element_id = entry.element;
    m_fragment_output_queues[key] = std::unique_ptr<fragmentsink_t>(new fragmentsink_t(entry.queueinstance));
  }

  m_queue_timeout = std::chrono::milliseconds(parsed_conf.general_queue_timeout);
  m_connection_name = parsed_conf.connection_name;

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
FragmentReceiver::do_start(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  m_received_fragments = 0;
  m_received_fragments_by_geoid.clear();
  m_run_number = payload.value<dunedaq::dataformats::run_number_t>("run", 0);

  networkmanager::NetworkManager::get().start_listening(
    m_connection_name, std::bind(&FragmentReceiver::dispatch_fragment, this, std::placeholders::_1));

  TLOG() << get_name() << " successfully started for run number " << m_run_number;
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
FragmentReceiver::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  networkmanager::NetworkManager::get().stop_listening(m_connection_name);

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
FragmentReceiver::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  fragmentreceiverinfo::Info info;
  info.fragments_received = m_received_fragments;
  // TODO, Eric Flumerfelt <eflumerf@fnal.gov>, 07-Oct-2021: Is this something we want to support?
  // for (auto& component : m_received_fragments_by_geoid) {
  //   fragmentreceiverinfo::Component comp;
  //   comp.system = dataformats::GeoID::system_type_to_string(component.first.system_type);
  //   comp.region = component.first.region_id;
  //   comp.element = component.first.element_id;
  //   comp.fragments_received = component.second;
  //   info.components.push_back(comp);
  // }
  ci.add(info);
}

void
FragmentReceiver::dispatch_fragment(ipm::Receiver::Response message)
{
  auto fragment = serialization::deserialize<std::unique_ptr<dataformats::Fragment>>(message.data);

  auto component = fragment->get_element_id();
  if (m_fragment_output_queues.count(component)) {
    m_fragment_output_queues.at(component)->push(std::move(fragment), m_queue_timeout);
  } else {
    throw UnknownGeoID(ERS_HERE, component);
  }
  m_received_fragments_by_geoid[component]++;
  m_received_fragments++;
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::FragmentReceiver)
