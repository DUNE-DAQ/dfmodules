/**
 * @file FragmentAggregatorModule.cpp FragmentAggregatorModule implementation
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FragmentAggregatorModule.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/opmon/FragmentAggregatorModule.pb.h"

#include "appmodel/FragmentAggregatorModule.hpp"
#include "confmodel/Connection.hpp"
#include "confmodel/QueueWithSourceId.hpp"
#include "daqdataformats/FragmentHeader.hpp"
#include "dfmessages/Fragment_serialization.hpp"
#include "logging/Logging.hpp"

#include "iomanager/IOManager.hpp"

#include <iostream>
#include <string>

namespace dunedaq {
namespace dfmodules {

FragmentAggregatorModule::FragmentAggregatorModule(const std::string& name)
  : DAQModule(name)
{
  register_command("start", &FragmentAggregatorModule::do_start);
  register_command("stop_trigger_sources", &FragmentAggregatorModule::do_stop);
}

void
FragmentAggregatorModule::init(std::shared_ptr<appfwk::ConfigurationManager> mcfg)
{
  auto mdal = mcfg->get_dal<appmodel::FragmentAggregatorModule>(get_name());
  if (!mdal) {
    throw appfwk::CommandFailed(ERS_HERE, "init", get_name(), "Unable to retrieve configuration object");
  }

  auto inputs = mdal->get_inputs();
  for (auto con : mdal->get_inputs()) {
    if (con->get_data_type() == datatype_to_string < dfmessages::DataRequest>()) {
      m_data_req_input = con->UID();
    }
    if (con->get_data_type() == datatype_to_string<daqdataformats::Fragment>()) {
      m_fragment_input = con->UID();
    }
  }

  m_producer_conn_ids.clear();
  for (const auto cr : mdal->get_outputs()) {
    if (cr->get_data_type() == datatype_to_string<dfmessages::DataRequest>()) {
	auto qid = cr->cast<confmodel::QueueWithSourceId>();
      	    m_producer_conn_ids[qid->get_source_id()] = cr->UID();
    }
    if (cr->get_data_type() == datatype_to_string<std::unique_ptr<daqdataformats::Fragment>>()) {
      m_trb_conn_ids.push_back(cr->UID());
    }
  }

  // this is just to get the data request receiver registered early (before Start)
  auto iom = iomanager::IOManager::get();
  iom->get_receiver<dfmessages::DataRequest>(m_data_req_input);
}

void
FragmentAggregatorModule::generate_opmon_data()
{
  if (m_data_requests_processed > 0) {
    opmon::FragmentAggregatorTimeInfo dr_times;
    dr_times.set_min_us(m_data_requests_time_min_us.exchange(std::numeric_limits<uint64_t>::max()));	  
    dr_times.set_max_us(m_data_requests_time_max_us.exchange(0));
    dr_times.set_average_us(m_data_requests_time_average_us.exchange(0) / m_data_requests_processed);
    this->publish(std::move(dr_times), { { "data", "DataRequest" } });
  }

  if (m_fragments_processed > 0) {
    opmon::FragmentAggregatorTimeInfo frag_times;
    frag_times.set_min_us(m_fragments_time_min_us.exchange(std::numeric_limits<uint64_t>::max()));
    frag_times.set_max_us(m_fragments_time_max_us.exchange(0));
    frag_times.set_average_us(m_fragments_time_average_us.exchange(0) / m_fragments_processed);
    this->publish(std::move(frag_times), { { "data", "Fragment" } });
  }

  opmon::FADataRequestsCounterInfo dr_info;
  dr_info.set_data_requests_received(m_data_requests_received.exchange(0));
  dr_info.set_data_requests_processed(m_data_requests_processed.exchange(0));
  dr_info.set_data_requests_failed(m_data_requests_failed.load());  //the failed counters are meant NOT to reset
  this->publish(std::move(dr_info));

  opmon::FAFragmentsCounterInfo frag_info;
  frag_info.set_fragments_received(m_fragments_received.exchange(0));
  frag_info.set_fragments_processed(m_fragments_processed.exchange(0));
  frag_info.set_fragments_failed(m_fragments_failed.load());
  frag_info.set_fragments_empty(m_fragments_empty.exchange(0));
  frag_info.set_fragments_incomplete(m_fragments_incomplete.exchange(0));
  frag_info.set_fragments_invalid(m_fragments_invalid.exchange(0));
  this->publish(std::move(frag_info));
}

void
FragmentAggregatorModule::do_start(const data_t& /* args */)
{

  m_data_requests_received.store(0);
  m_data_requests_processed.store(0);
  m_data_requests_failed.store(0);
  m_fragments_received.store(0);
  m_fragments_processed.store(0);
  m_fragments_failed.store(0);
  m_fragments_empty.store(0);
  m_fragments_incomplete.store(0);
  m_fragments_invalid.store(0);
  m_fragments_time_average_us.store(0);
  m_fragments_time_min_us.store(std::numeric_limits<uint64_t>::max());
  m_fragments_time_max_us.store(0);
  m_data_requests_time_average_us.store(0);
  m_data_requests_time_min_us.store(std::numeric_limits<uint64_t>::max());
  m_data_requests_time_max_us.store(0);

  // 19-Dec-2024, KAB: check that Fragment senders are ready to send. This is done so
  // that the IOManager infrastructure fetches the necessary connection details from
  // the ConnectivityService at 'start' time, instead of the first time that the sender
  // is used to send data.  This avoids delays in the sending of the first fragment in
  // the first data-taking run in a DAQ session. Such delays can lead to undesirable
  // system behavior like trigger inhibits.
  auto iom = iomanager::IOManager::get();
  for (auto trb_conn : m_trb_conn_ids) {
    auto sender = iom->get_sender<std::unique_ptr<daqdataformats::Fragment>>(trb_conn);
    if (sender != nullptr) {
      bool is_ready = sender->is_ready_for_sending(std::chrono::milliseconds(100));
      TLOG_DEBUG(0) << "The Fragment sender for " << trb_conn << " " << (is_ready ? "is" : "is not") << " ready.";
    }
  }
  iom->add_callback<dfmessages::DataRequest>(
    m_data_req_input, std::bind(&FragmentAggregatorModule::process_data_request, this, std::placeholders::_1));
  iom->add_callback<std::unique_ptr<daqdataformats::Fragment>>(
    m_fragment_input, std::bind(&FragmentAggregatorModule::process_fragment, this, std::placeholders::_1));
}

void
FragmentAggregatorModule::do_stop(const data_t& /* args */)
{
  auto iom = iomanager::IOManager::get();
  iom->remove_callback<dfmessages::DataRequest>(m_data_req_input);
  iom->remove_callback<std::unique_ptr<daqdataformats::Fragment>>(m_fragment_input);
  m_data_req_map.clear();
}

void
FragmentAggregatorModule::process_data_request(dfmessages::DataRequest& data_request)
{

  {
    std::scoped_lock lock(m_mutex);
    
    m_timestamp_before_dr = get_current_time_us();
    m_data_requests_received++;

    std::tuple<dfmessages::trigger_number_t, dfmessages::sequence_number_t, daqdataformats::SourceID> triplet = {
      data_request.trigger_number, data_request.sequence_number, data_request.request_information.component
    };
    m_data_req_map[triplet] = data_request.data_destination;
  }
  // Forward Data Request to the right DLH
  try {
    //std::string component_name = "inputReqToDLH-" + data_request.request_information.component.to_string();
    auto uid_elem = m_producer_conn_ids.find(data_request.request_information.component.id);
    if (uid_elem == m_producer_conn_ids.end()) {
      ers::error(dunedaq::dfmodules::DRSenderLookupFailed(ERS_HERE,
                                                          data_request.request_information.component,
                                                          data_request.run_number,
                                                          data_request.trigger_number,
                                                          data_request.sequence_number));
    } else {
      TLOG_DEBUG(30) << "Send data request to " << uid_elem->second;
      auto sender = get_iom_sender<dfmessages::DataRequest>(uid_elem->second);
      data_request.data_destination = m_fragment_input;
      sender->send(std::move(data_request), iomanager::Sender::s_no_block);

      m_data_requests_processed++;
      auto timestamp_total = get_current_time_us() - m_timestamp_before_dr;
      if (timestamp_total < m_data_requests_time_min_us) { m_data_requests_time_min_us = timestamp_total; }
      if (timestamp_total > m_data_requests_time_max_us) { m_data_requests_time_max_us = timestamp_total; }
      m_data_requests_time_average_us += timestamp_total;
    }
  } catch (const ers::Issue& excpt) {
    ers::warning(excpt);
    m_data_requests_failed++;
  }
}

void
FragmentAggregatorModule::process_fragment(std::unique_ptr<daqdataformats::Fragment>& fragment)
{
  // Forward Fragment to the right TRB
  std::string trb_identifier;
  {
    std::scoped_lock lock(m_mutex);
    
    m_timestamp_before_frag = get_current_time_us();
    m_fragments_received++;

    std::bitset<32> error_bits = fragment->get_error_bits();
    if (error_bits[static_cast<size_t>(dunedaq::daqdataformats::FragmentErrorBits::kDataNotFound)])
      m_fragments_empty++;
    if (error_bits[static_cast<size_t>(dunedaq::daqdataformats::FragmentErrorBits::kIncomplete)])
      m_fragments_incomplete++;
    if (error_bits[static_cast<size_t>(dunedaq::daqdataformats::FragmentErrorBits::kInvalidWindow)])
      m_fragments_invalid++;

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
    TLOG_DEBUG(27) << get_name() << " Sending fragment for trigger/sequence_number "
                   << fragment->get_trigger_number() << "."
                   << fragment->get_sequence_number() << " and SourceID "
                   << fragment->get_element_id() << " to "
                   << trb_identifier;
    auto sender = get_iom_sender<std::unique_ptr<daqdataformats::Fragment>>(trb_identifier);
    sender->send(std::move(fragment), iomanager::Sender::s_no_block);
    
    m_fragments_processed++;
    auto timestamp_total = get_current_time_us() - m_timestamp_before_frag;
    if (timestamp_total < m_fragments_time_min_us) { m_fragments_time_min_us = timestamp_total; }
    if (timestamp_total > m_fragments_time_max_us) { m_fragments_time_max_us = timestamp_total; }
    m_fragments_time_average_us += timestamp_total;

  } catch (const ers::Issue& excpt) {
    ers::error(AbandonedFragment(ERS_HERE,
				 fragment->get_run_number(),
				 fragment->get_trigger_number(),
				 fragment->get_sequence_number(),
				 fragment->get_element_id(),
				 excpt));
    m_fragments_failed++;
  }
}

uint64_t 
FragmentAggregatorModule::get_current_time_us()
{
  return std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::FragmentAggregatorModule)
