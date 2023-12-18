/**
 * @file FragmentAggregator.cpp FragmentAggregator implementation
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FragmentAggregator.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "appdal/FragmentAggregator.hpp"
#include "appfwk/app/Nljs.hpp"
#include "coredal/Connection.hpp"
#include "dfmessages/Fragment_serialization.hpp"
#include "logging/Logging.hpp"

#include "iomanager/IOManager.hpp"

#include <iostream>
#include <string>

namespace dunedaq {
namespace dfmodules {

FragmentAggregator::FragmentAggregator(const std::string& name)
  : DAQModule(name)
{
  register_command("start", &FragmentAggregator::do_start);
  register_command("stop_trigger_sources", &FragmentAggregator::do_stop);
}

void
FragmentAggregator::init(std::shared_ptr<appfwk::ModuleConfiguration> mcfg)
{
  auto mdal = mcfg->module<appdal::FragmentAggregator>(get_name());

  auto inputs = mdal->get_inputs();
  for (auto con : mdal->get_inputs()) {
    if (con->get_data_type() == "dfmessages::DataRequest") {
      m_data_req_input = con->UID();
    }
    if (con->get_data_type() == "daqdataformats::Fragment") {
      m_fragment_input = con->UID();
    }
  }

  m_producer_conn_ids.clear();
  for (const auto cr : mdal->get_outputs()) {
    if (cr->get_data_type() == "dfmessages::DataRequest") {
      m_producer_conn_ids.insert(cr->UID());
    }
  }
}

void
FragmentAggregator::get_info(opmonlib::InfoCollector& /*ci*/, int /* level */)
{
  // dummyconsumerinfo::Info info;
  // info.packets_processed = m_packets_processed;

  // ci.add(info);
}

void
FragmentAggregator::do_start(const data_t& /* args */)
{
  m_packets_processed = 0;
  auto iom = iomanager::IOManager::get();
  iom->add_callback<dfmessages::DataRequest>(
    m_data_req_input, std::bind(&FragmentAggregator::process_data_request, this, std::placeholders::_1));
  iom->add_callback<std::unique_ptr<daqdataformats::Fragment>>(
    m_fragment_input, std::bind(&FragmentAggregator::process_fragment, this, std::placeholders::_1));
}

void
FragmentAggregator::do_stop(const data_t& /* args */)
{
  auto iom = iomanager::IOManager::get();
  iom->remove_callback<dfmessages::DataRequest>(m_data_req_input);
  iom->remove_callback<std::unique_ptr<daqdataformats::Fragment>>(m_fragment_input);
  m_data_req_map.clear();
}

void
FragmentAggregator::process_data_request(dfmessages::DataRequest& data_request)
{

  {
    std::scoped_lock lock(m_mutex);
    std::tuple<dfmessages::trigger_number_t, dfmessages::sequence_number_t, daqdataformats::SourceID> triplet = {
      data_request.trigger_number, data_request.sequence_number, data_request.request_information.component
    };
    m_data_req_map[triplet] = data_request.data_destination;
  }
  // Forward Data Request to the right DLH
  try {
    std::string component_name = "inputReqToDLH-" + data_request.request_information.component.to_string();
    auto uid_elem = m_producer_conn_ids.find(component_name);
    if (uid_elem == m_producer_conn_ids.end()) {
      ers::error(dunedaq::dfmodules::DRSenderLookupFailed(ERS_HERE,
                                                          data_request.request_information.component,
                                                          data_request.run_number,
                                                          data_request.trigger_number,
                                                          data_request.sequence_number));
    } else {
      std::string uid = *uid_elem;
      auto sender = get_iom_sender<dfmessages::DataRequest>(uid);
      data_request.data_destination = "fragment_queue";
      sender->send(std::move(data_request), iomanager::Sender::s_no_block);
    }
  } catch (const ers::Issue& excpt) {
    ers::warning(excpt);
  }
}

void
FragmentAggregator::process_fragment(std::unique_ptr<daqdataformats::Fragment>& fragment)
{
  // Forward Fragment to the right TRB
  std::string trb_identifier;
  {
    std::scoped_lock lock(m_mutex);
    auto dr_iter = m_data_req_map.find(
      std::make_tuple<dfmessages::trigger_number_t, dfmessages::sequence_number_t, daqdataformats::SourceID>(
        fragment->get_trigger_number(), fragment->get_sequence_number(), fragment->get_element_id()));
    if (dr_iter != m_data_req_map.end()) {
      trb_identifier = dr_iter->second;
      m_data_req_map.erase(dr_iter);
    } else {
      ers::error(UnknownFragmentDestination(
        ERS_HERE, fragment->get_trigger_number(), fragment->get_sequence_number(), fragment->get_element_id()));
      return;
    }
  }
  try {
    auto sender = get_iom_sender<std::unique_ptr<daqdataformats::Fragment>>(trb_identifier);
    sender->send(std::move(fragment), iomanager::Sender::s_no_block);
  } catch (const ers::Issue& excpt) {
    ers::warning(excpt);
  }
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::FragmentAggregator)
