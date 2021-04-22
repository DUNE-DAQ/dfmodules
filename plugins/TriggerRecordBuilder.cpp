/**
 * @file TriggerRecordBuilder.cpp TriggerRecordBuilder class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "TriggerRecordBuilder.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/app/Nljs.hpp"
#include "dfmodules/triggerrecordbuilder/Nljs.hpp"
#include "dfmodules/triggerrecordbuilder/Structs.hpp"
#include "logging/Logging.hpp"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief TRACE debug levels used in this source file
 */
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_WORK_STEPS = 10,
  TLVL_BOOKKEEPING = 15
};

namespace dunedaq {
namespace dfmodules {

using dataformats::TriggerRecordErrorBits;

TriggerRecordBuilder::TriggerRecordBuilder(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_thread(std::bind(&TriggerRecordBuilder::do_work, this, std::placeholders::_1))
  , m_queue_timeout(100)
{

  register_command("conf", &TriggerRecordBuilder::do_conf);
  register_command("start", &TriggerRecordBuilder::do_start);
  register_command("stop", &TriggerRecordBuilder::do_stop);
}

void
TriggerRecordBuilder::init(const data_t& init_data)
{

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  //--------------------------------
  // Get single queues
  //---------------------------------

  auto qi = appfwk::queue_index(init_data, { "trigger_decision_input_queue", "trigger_record_output_queue" });
  // data request input queue
  try {
    auto temp_info = qi["trigger_decision_input_queue"];
    std::string temp_name = temp_info.inst;
    trigger_decision_source_t test(temp_name);
    m_trigger_decision_source_name = temp_name;
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_input_queue", excpt);
  }

  // trigger record output
  try {
    auto temp_info = qi["trigger_record_output_queue"];
    std::string temp_name = temp_info.inst;
    trigger_record_sink_t test(temp_name);
    m_trigger_record_sink_name = temp_name;
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_record_output_queue", excpt);
  }

  //----------------------
  // Get dynamic queues
  //----------------------

  // set names for the fragment queue(s)
  auto ini = init_data.get<appfwk::app::ModInit>();

  // get the names for the fragment queues
  for (const auto& qitem : ini.qinfos) {
    if (qitem.name.rfind("data_fragment_") == 0) {
      try {
        std::string temp_name = qitem.inst;
        fragment_source_t test(temp_name);
        m_fragment_source_names.push_back(temp_name);
      } catch (const ers::Issue& excpt) {
        throw InvalidQueueFatalError(ERS_HERE, get_name(), qitem.name, excpt);
      }
    }
  }

  // Test for valid output data request queues
  auto ini = init_data.get<appfwk::app::ModInit>();
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
TriggerRecordBuilder::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{

  triggerrecordbuilderinfo::Info i;

  i.trigger_decisions = m_trigger_decisions_counter.load();
  i.populated_trigger_ids = m_fragment_index_counter.load();
  i.old_trigger_ids = m_old_fragment_index_counter.load();
  i.total_fragments = m_fragment_counter.load();
  i.old_fragments = m_old_fragment_counter.load();

  ci.add(i);
}

void
TriggerRecordBuilder::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  m_map_geoid_queues.clear() ;

  triggerrecordbuilder::ConfParams parsed_conf = payload.get<triggerrecordbuilder::ConfParams>();

  for (auto const& entry : parsed_conf.map) {
    dataformats::GeoID key;
    key.apa_number = entry.apa;
    key.link_number = entry.link;
    m_map_geoid_queues[key] = entry.queueinstance;
  }
  
  m_max_time_difference = parsed_conf.max_timestamp_diff;
  
  m_queue_timeout = std::chrono::milliseconds(parsed_conf.general_queue_timeout);
  
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}
  
void
TriggerRecordBuilder::do_start(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  m_thread.start_working_thread(get_name());
  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
TriggerRecordBuilder::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  m_thread.stop_working_thread();
  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
TriggerRecordBuilder::do_work(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  // uint32_t receivedCount = 0;

  // clean books from possible previous memory
  m_trigger_records.clear() ;

  // allocate queues
  trigger_decision_source_t decision_source(m_trigger_decision_source_name);
  trigger_record_sink_t record_sink(m_trigger_record_sink_name);
  std::vector<std::unique_ptr<fragment_source_t>> frag_sources;
  for (unsigned int i = 0; i < m_fragment_source_names.size(); ++i) {
    frag_sources.push_back(std::unique_ptr<fragment_source_t>(new fragment_source_t(m_fragment_source_names[i])));
  }

  datareqsinkmap_t request_sinks ;
  for (auto const& entry : m_map_geoid_queues) {
    request_sinks[entry.first] = std::unique_ptr<datareqsink_t>(new datareqsink_t(entry.second));
  }
  
  bool book_updates = false ;
  
  while (running_flag.load() || book_updates) {
    
    book_updates = false ;

    fill_counters();
    
    // read decision requests
    if (decision_source.can_pop()) {
      
      try {
	
	// get the trigger decision
	dfmessages::TriggerDecision temp_dec;
	decision_source.pop(temp_dec, m_queue_timeout);
	
	m_current_time = temp_dec.trigger_timestamp;
	
	// create the book entry
	TriggerId temp_id(temp_dec);
	
	auto it = find( temp_id ) ;
	if ( it != trigger_records.end() ) {
	  ers::error( ERS_HERE, DuplicatedTriggerDecision( temp_id ) ) ;
	}
	

	// create trigger record
	dataformats::TriggerRecord & temp_tr =
	  trigger_records[temp_id] = trigger_record_ptr_t( new dataformats::TriggerRecord(temp_dec.components) ) ; 

	temp_tr.get_header_ref().set_trigger_number(temp_dec.trigger_number);
	temp_tr.get_header_ref().set_run_number(temp_dec.run_number);
	temp_tr.get_header_ref().set_trigger_timestamp(temp_dec.trigger_timestamp);
	temp_tr.get_header_ref().set_trigger_type(temp_dec.trigger_type);
	
	book_updates = true ;
	
	// dispatch requests
	dispatch_data_requests( temp_dec, request_sinks ) const ;

      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) { ; }
      
    } // if we can pop a trigger decision
    
    
    // read the fragments queues
    book_updates = book_updates || read_fragments( frag_sources );
    
    //-------------------------------------------------
    // Check if trigger records are complete or timedout
    // and create dedicated record
    //--------------------------------------------------

    if (book_updates) {
      

      TLOG_DEBUG(TLVL_BOOKKEEPING) << "Bookeeping status: " << m_trigger_records.size() << " trigger records in progress " ;
      
      std::vector<TriggerId> complete;
      for (const auto& tr : m_trigger_records) {
	std::ostringstream message;
        message << tr.first << " with " << tr.second.components.size() << '/' 
		<< tr.second.get_header_ref().get_num_requested_components() << " components";
	
	if ( tr.second.components.size() >= tr.second.get_header_ref().get_num_requested_components() ) {

	  
	  // check GeoID matching

	  message << ": complete" ;
	  complete.push_back( tr.first ) ;
	  
	}
	
	TLOG_DEBUG(TLVL_BOOKKEEPING) << message.str();
	
      } // loop over TRs to check if they are complete
      
      //------------------------------------------------
      // Create TriggerRecords and send them
      //-----------------------------------------------

      for (const auto& id : complete) {
	
        send_trigger_record(id, record_sink, running_flag);
	
      } // loop over compled trigger id
      
      //-------------------------------------------------
      // Check if some fragments are obsolete
      //--------------------------------------------------
      check_stale_requests() ;
      
    } // if books were updated
    else {
      if (running_flag.load()) {
        std::this_thread::sleep_for(m_queue_timeout);
      }
    }

  } // working loop
  
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Starting draining phase ";
  std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
  
  // //-------------------------------------------------
  // // Here we drain what has been left from the running condition
  // //--------------------------------------------------
  
  // create all possible trigger record
  std::vector<TriggerId> triggers;
  for (const auto& entry : m_trigger_records) {
    triggers.push_back(entry.first);
  }
  
  // create the trigger record and send it
  for (const auto& t : triggers) {
    send_trigger_record(t, record_sink, running_flag);
  }
  
  fill_counters();

  std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();

  std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, " << m_trigger_decisions.size() << " reminaing Trigger Decision and "
           << m_fragments.size() << " remaining fragment stashes" << std::endl
           << "Draining took : " << time_span.count() << " s";
  TLOG() << ProgressUpdate(ERS_HERE, get_name(), oss_summ.str());

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

bool
TriggerRecordBuilder::read_fragments( fragment_sources_t& frag_sources, bool drain)
{

  bool book_updates = false;

  //-------------------------------------------------
  // Try to get Fragments from every queue
  //--------------------------------------------------

  for (unsigned int j = 0; j < frag_sources.size(); ++j) {
    
    while (frag_sources[j]->can_pop()) {
      
      std::unique_ptr<dataformats::Fragment> temp_fragment;

      try {
        frag_sources[j]->pop(temp_fragment, m_queue_timeout);
	
      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
        // it is perfectly reasonable that there might be no data in the queue
        // some fraction of the times that we check, so we just continue on and try again
        continue;
      }

      TriggerId temp_id(*temp_fragment);

      auto it = m_trigger_records.find( temp_id ) ;
      
      if ( it == m_trigger_records.end() ) {
	ers::error( UnexpectedFragment(  ERS_HERE,
					 temp_id, 
					 temp_fragment -> get_fragment_type(), 
					 temp_fragment -> get_link_id() ) ) ;
      }
      else {
	
	// check if the fragment has a GeoId that was desired
	dataformats::TriggerRecordHeader & header = it -> second.get_header_ref() ;
	
	bool requested = false ;
	for ( size_t i = 0 ; i < header.get_num_requested_components() ; ++i ) {
	  
	  const dataformats::ComponentRequest &  request = header[i] ;
	  if ( request.component == temp_fragment -> get_link_id() ) {
	    requested = true ;
	    break ;
	  }
	  
	} // request loop

	if ( requested ) {
	  it  -> add_fragment( std::move( temp_fragment ) ) ;
	  book_updates = true;
	}
	else {
	  ers::error( UnexpectedFragment( ERS_HERE, 
					  temp_id, 
					  temp_fragment -> get_fragment_type(), 
					  temp_fragment -> get_link_id() ) ) ;
	}
	
      } // if we can pop
      
      if (!drain)
        break;

    } // while loop over the j-th queue

  } // queue loop

  return book_updates;
}

trigger_record_ptr_t
TriggerRecordBuilder::extract_trigger_record(const TriggerId& id)
{

  auto it = m_trigger_records.find(id);

  trigger_record_ptr_t temp = std::move( it -> second ) ;
  
  m_trigger_records.erase( it ) ;

  if ( temp -> get_fragments_ref().size() < temp-> get_header_ref().get_num_requested_components() ) {
    temp->get_header_ref().set_error_bit(TriggerRecordErrorBits::kIncomplete, true);
  }
  
  return temp ;
}

bool
TriggerRecordBuilder::send_trigger_record(const TriggerId& id, trigger_record_sink_t& sink, std::atomic<bool>& running)
{

  trigger_record_ptr_t temp_record( extract_trigger_record(id) );

  bool wasSentSuccessfully = false;
  while (!wasSentSuccessfully) {
    try {
      sink.push(std::move(temp_record), m_queue_timeout);
      wasSentSuccessfully = true;
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      std::ostringstream oss_warn;
      oss_warn << "push to output queue \"" << get_name() << "\"";
      ers::warning(dunedaq::appfwk::QueueTimeoutExpired(
        ERS_HERE,
        sink.get_name(),
        oss_warn.str(),
        std::chrono::duration_cast<std::chrono::milliseconds>(m_queue_timeout).count()));
    }

    if (!running.load())
      break;
  } // push while loop

  return wasSentSuccessfully;
}

bool
TriggerRecordBuilder::check_old_fragments() const
{

  bool old_stuff = false;

  metric_counter_type old_fragments = 0;
  metric_counter_type old_trigger_indexes = 0;

  for (auto it = m_fragments.begin(); it != m_fragments.end(); ++it) {

    metric_counter_type index_old_fragments = 0;

    for (auto frag_it = it->second.begin(); frag_it != it->second.end(); ++frag_it) {

      if (m_current_time > m_max_time_difference + (*frag_it)->get_trigger_timestamp()) {
        old_stuff = true;
        ++index_old_fragments;
        ers::error(FragmentObsolete(ERS_HERE,
                                    (*frag_it)->get_trigger_number(),
                                    (*frag_it)->get_fragment_type_code(),
                                    (*frag_it)->get_trigger_timestamp(),
                                    m_current_time));
        // it = m_trigger_decisions.erase( it ) ;

        // note that if we reached this point it means there is no corresponding trigger decision for this id
        // otherwise we would have created a dedicated trigger record (though probably incomplete)
        // so there is no need to check the trigger decision book
      }
    } // vector loop

    if (index_old_fragments > 0) {
      ++old_trigger_indexes;
      old_fragments += index_old_fragments;
    }
  } // fragment loop

  m_old_fragment_counter.store(old_fragments);
  m_old_fragment_index_counter.store(old_trigger_indexes);

  return old_stuff;
}

void
TriggerRecordBuilder::fill_counters() const
{

  m_trigger_decisions_counter.store(m_trigger_decisions.size());
  m_fragment_index_counter.store(m_fragments.size());
  metric_counter_type tot = std::accumulate(
    m_fragments.begin(), m_fragments.end(), 0, [](auto tot, auto& ele) { return tot += ele.second.size(); });
  m_fragment_counter.store(tot);
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::TriggerRecordBuilder)
