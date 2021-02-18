/**
 * @file FragmentReceiver.cpp FragmentReceiver class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FragmentReceiver.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "TRACE/trace.h"
#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/cmd/Nljs.hpp"
#include "dfmodules/fragmentreceiver/Nljs.hpp"
#include "dfmodules/fragmentreceiver/Structs.hpp"
#include "ers/ers.h"

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
#define TRACE_NAME "FragmentReceiver"          // NOLINT
#define TLVL_ENTER_EXIT_METHODS TLVL_DEBUG + 5 // NOLINT
#define TLVL_WORK_STEPS TLVL_DEBUG + 10        // NOLINT
#define TLVL_BOOKKEEPING TLVL_DEBUG + 15       // NOLINT

namespace dunedaq {
namespace dfmodules {

  using dataformats::TriggerRecordErrorBits ;


FragmentReceiver::FragmentReceiver(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_thread(std::bind(&FragmentReceiver::do_work, this, std::placeholders::_1))
  , m_queue_timeout(100)
{

  register_command("conf", &FragmentReceiver::do_conf);
  register_command("start", &FragmentReceiver::do_start);
  register_command("stop", &FragmentReceiver::do_stop);
}

void
FragmentReceiver::init(const data_t& init_data)
{

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

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

  // set names for the
  auto ini = init_data.get<appfwk::cmd::ModInit>();

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

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
FragmentReceiver::do_conf(const data_t& payload)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  fragmentreceiver::ConfParams parsed_conf = payload.get<fragmentreceiver::ConfParams>();

  m_max_time_difference = parsed_conf.max_timestamp_diff;

  m_queue_timeout = std::chrono::milliseconds(parsed_conf.general_queue_timeout);

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
FragmentReceiver::do_start(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  m_thread.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
FragmentReceiver::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  m_thread.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
FragmentReceiver::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  // uint32_t receivedCount = 0;
  
  // clean books from possible previous memory
  m_trigger_decisions.clear() ;
  m_fragments.clear() ;

  // allocate queues
  trigger_decision_source_t decision_source(m_trigger_decision_source_name);
  trigger_record_sink_t record_sink(m_trigger_record_sink_name);
  std::vector<std::unique_ptr<fragment_source_t>> frag_sources;
  for (unsigned int i = 0; i < m_fragment_source_names.size(); ++i) {
    frag_sources.push_back(std::unique_ptr<fragment_source_t>(new fragment_source_t(m_fragment_source_names[i])));
  }

  bool book_updates = false ; 
  
  while (running_flag.load() || book_updates ) {

    book_updates =  read_queues( decision_source, frag_sources ) ;

    // TLOG(TLVL_WORK_STEPS) << "Decision size: " << m_trigger_decisions.size() ;
    // TLOG(TLVL_WORK_STEPS) << "Frag size: " << m_fragments.size() ;

    //-------------------------------------------------
    // Check if some decisions are complete or timedout
    // and create dedicated record
    //--------------------------------------------------

    if (book_updates) {

      std::ostringstream message;
      TLOG(TLVL_BOOKKEEPING) << "Bookeeping status: " << m_trigger_decisions.size() << " decisions and "
                             << m_fragments.size() << " Fragment stashes";
      message << "Trigger Decisions: ";

      for (const auto& d : m_trigger_decisions) {
        message << d.first << " with " << d.second.components.size() << " components, ";
      }
      TLOG(TLVL_BOOKKEEPING) << message.str();
      message.str("");
      message << "Fragments: ";
      for (const auto& f : m_fragments) {
        message << f.first << " with " << f.second.size() << " fragments, ";
      }

      TLOG(TLVL_BOOKKEEPING) << message.str();
      // ers::info(ProgressUpdate(ERS_HERE, get_name(), message.str()));

      std::vector<TriggerId> complete;

      for (auto it = m_trigger_decisions.begin(); it != m_trigger_decisions.end(); ++it) {

        // if ( current_time - it -> second.trigger_timestamp > m_max_time_difference ) {
        // 	ers::warning( TimedOutTriggerDecision( ERS_HERE, it -> second, current_time ) ) ;
        // 	temp_record = BuildTriggerRecord( it -> first ) ;
        // }

        auto frag_it = m_fragments.find(it->first);

        if (frag_it != m_fragments.end()) {

          if (frag_it->second.size() >= it->second.components.size()) {
            complete.push_back(it->first);
          } else {
            // std::ostringstream message ;
            TLOG(TLVL_WORK_STEPS) << "Trigger decision " << it->first << " status: " << frag_it->second.size() << " / "
                                  << it->second.components.size() << " Fragments";

            // ers::error(ProgressUpdate(ERS_HERE, get_name(), message.str()));
          }
        }

      } // decision loop for complete record

      //------------------------------------------------
      // Create TriggerRecords and send them
      //-----------------------------------------------

      for (const auto& id : complete) {
	
	send_trigger_record( id, record_sink, running_flag ) ;
	
      }   // loop over compled trigger id
      
      //-------------------------------------------------
      // Check if some fragments are obsolete
      //--------------------------------------------------

      for (auto it = m_fragments.begin(); it != m_fragments.end(); ++it) {

        for (auto frag_it = it->second.begin(); frag_it != it->second.end(); ++frag_it) {

          if ( m_current_time > m_max_time_difference + (*frag_it)->get_trigger_timestamp()) {

            ers::error(FragmentObsolete(ERS_HERE,
                                        (*frag_it)->get_trigger_number(),
                                        (*frag_it)->get_fragment_type(),
                                        (*frag_it)->get_trigger_timestamp(),
                                        m_current_time));
            // it = m_trigger_decisions.erase( it ) ;

            // note that if we reached this point it means there is no corresponding trigger decision for this id
            // otherwise we would have created a dedicated trigger record (though probably incomplete)
            // so there is no need to check the trigger decision book
          }
        } // vector loop
      }   // fragment loop

    } // if a books were updated

  } // working loop

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Starting draining phase ";
  std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();

  // //-------------------------------------------------
  // // Here we drain what has been left from the running condition
  // //--------------------------------------------------
  // read_queues( decision_source, frag_sources, true ) ; 
    

  // create all possible trigger record
  std::vector<TriggerId> triggers ;
  for ( const auto & entry : m_trigger_decisions ) {
    triggers.push_back( entry.first ) ;
  }

  // create the trigger record and send it
  for ( const auto & t : triggers ) {
    send_trigger_record( t, record_sink, running_flag ) ;
  }

  
  std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();

  std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, " << m_trigger_decisions.size() << " reminaing Trigger Decision and "
           << m_fragments.size() << " remaining fragment stashes" << std::endl 
	   << "Draining took : " << time_span.count() << " s" ;
  ers::log(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

bool 
FragmentReceiver::read_queues( trigger_decision_source_t &  decision_source,  
			       fragment_sources_t & frag_sources,  
			       bool drain ) {

  bool book_updates = false ;

  // temp memory allocations
  dfmessages::TriggerDecision temp_dec;

  //-------------------------------------------------
  // Try to get a trigger decision of trigger decisions
  //--------------------------------------------------
  
  while ( decision_source.can_pop() ) {

    try {
      decision_source.pop(temp_dec, m_queue_timeout);
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      // it is perfectly reasonable that there might be no data in the queue
      // some fraction of the times that we check, so we just continue on and try again
      continue;
    }
    
    m_current_time = temp_dec.trigger_timestamp;
    
    TriggerId temp_id(temp_dec);
    m_trigger_decisions[temp_id] = temp_dec;
    
    book_updates = true;

    if ( ! drain ) break ;
    
  } // while decisions loop
  
    //-------------------------------------------------
    // Try to get Fragments from every queue
    //--------------------------------------------------
  
  for (unsigned int j = 0; j < frag_sources.size(); ++j) {
    
    while ( frag_sources[j]->can_pop() ) {

      std::unique_ptr<dataformats::Fragment> temp_fragment;
      
      try {
	frag_sources[j]->pop(temp_fragment, m_queue_timeout);
	
      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
	// it is perfectly reasonable that there might be no data in the queue
	// some fraction of the times that we check, so we just continue on and try again
	continue;
      }
      
      TriggerId temp_id(*temp_fragment);
      m_fragments[temp_id].push_back(std::move(temp_fragment));
      
      book_updates = true;
      
      if ( ! drain ) break ;
      
    } // while loop over the j-th queue

  } // queue loop

  return book_updates ;
}


dataformats::TriggerRecord*
FragmentReceiver::build_trigger_record(const TriggerId& id)
{

  auto trig_dec_it = m_trigger_decisions.find(id);
  const dfmessages::TriggerDecision& trig_dec = trig_dec_it->second;

  dataformats::TriggerRecord* trig_rec_ptr = new dataformats::TriggerRecord(trig_dec.components);

  trig_rec_ptr->get_header_ref().set_trigger_number(trig_dec.trigger_number);
  trig_rec_ptr->get_header_ref().set_run_number(trig_dec.run_number);
  trig_rec_ptr->get_header_ref().set_trigger_timestamp(trig_dec.trigger_timestamp);
  trig_rec_ptr->get_header_ref().set_trigger_type( trig_dec.trigger_type ) ;
  
  auto frags_it = m_fragments.find(id);
  auto& frags = frags_it->second;

  if (trig_dec.components.size() != frags.size()) {
    trig_rec_ptr->get_header_ref().set_error_bit( TriggerRecordErrorBits::kIncomplete, true) ;
  }

  while (frags.size() > 0) {
    trig_rec_ptr->add_fragment(std::move(frags.back()));
    frags.pop_back();
  }

  m_trigger_decisions.erase(trig_dec_it);
  m_fragments.erase(frags_it);

  return trig_rec_ptr;
}


bool
FragmentReceiver::send_trigger_record(const TriggerId& id , trigger_record_sink_t & sink,
				      std::atomic<bool> & running ) {
  
  std::unique_ptr<dataformats::TriggerRecord> temp_record(build_trigger_record(id));
    
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

    if ( ! running.load() ) break ;
  } // push while loop
  
  return wasSentSuccessfully ;

}


} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::FragmentReceiver)
