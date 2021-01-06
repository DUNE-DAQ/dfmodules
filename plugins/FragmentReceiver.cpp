/**
 * @file FragmentReceiver.cpp FragmentReceiver class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FragmentReceiver.hpp"
#include "CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
//#include "dfmodules/fakedataprod/Nljs.hpp"

#include "TRACE/trace.h"
#include "ers/ers.h"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "FragmentReceiver"  // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10 // NOLINT
#define TLVL_WORK_STEPS 15         // NOLINT

namespace dunedaq {
namespace dfmodules {

FragmentReceiver::FragmentReceiver(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&FragmentReceiver::do_work, this, std::placeholders::_1))
  , queueTimeout_(100)
  , dataRequestInputQueue_(nullptr)
  , dataFragmentOutputQueue_(nullptr)
{
  register_command("conf", &FragmentReceiver::do_conf);
  register_command("start", &FragmentReceiver::do_start);
  register_command("stop", &FragmentReceiver::do_stop);
}

void
FragmentReceiver::init(const data_t& init_data)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  // set names for the 

  auto qi = appfwk::qindex(init_data, { "data_request_input_queue", "data_fragment_output_queue" });
  try {
    dataRequestInputQueue_.reset(new datareqsource_t(qi["data_request_input_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "data_request_input_queue", excpt);
  }
  try {
    dataFragmentOutputQueue_.reset(new datafragsink_t(qi["data_fragment_output_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "data_fragment_output_queue", excpt);
  }
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
FragmentReceiver::do_conf(const data_t& /*payload*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  // fakedataprod::Conf tmpConfig = payload.get<fakedataprod::Conf>();
  // sleepMsecWhileRunning_ = tmpConfig.sleep_msec_while_running;

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
FragmentReceiver::do_start(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  thread_.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
FragmentReceiver::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  thread_.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
FragmentReceiver::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  //uint32_t receivedCount = 0;

  // allocate queues
  trigger_decision_source_t decision_source( trigger_decision_source_name_ ) ;
  trigger_record_sink_t record_sink( trigger_record_sink_name_ ) ;
  std::vector<std::unique_ptr<fragment_source_t>> frag_sources ; 
  for ( unsigned int i = 0 ; i < fragment_source_names_.size() ; ++i ) {
    frag_sources.push_back( new fragment_source_t( fragment_source_names_[i] ) ) ; 
  }
    
  timestamp_t current_time = 0 ; 
  
  // temp memory allocations
  dfmessages::TriggerDecision  temp_dec ;
  std::unique_ptr<dataformats::Fragment> temp_fragment ; 
  TriggerId temp_id ;

  while (running_flag.load()) {

    //-------------------------------------------------
    // Retrieve a certain number of trigger decisions
    //--------------------------------------------------

    for ( unsigned int i = 0 ; i < decision_loop_cnt_ ; ++i ) {
      
      try {
	decision_source.pop( temp_dec, trigger_decision_timeout_ ) ;
	
      } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
	// it is perfectly reasonable that there might be no data in the queue
	// some fraction of the times that we check, so we just continue on and try again
	continue;
      }

      current_time = temp_dec.trigger_timestamp ; 

      temp_id = TriggerId( temp_dec ) ;
      trigger_decisions_[temp_id] = temp_dec ; 
      
    }  // decision loop
    
    //-------------------------------------------------
    // Try to get Fragments from every queue 
    //--------------------------------------------------
    
    for ( unsigned int i = 0 ; i < fragment_loop_cnt_ ; ++i ) {
      
      for ( unsigned int j = 0 ; j < frag_sources.size() ; ++j ) {

	try {
	  frag_sources[j] -> pop( temp_fragment, fragment_timeout_) ;
	
	} catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
	// it is perfectly reasonable that there might be no data in the queue
	// some fraction of the times that we check, so we just continue on and try again
	  continue;
	}
	
	temp_id  = TriggerId( *temp_fragment ) ;
	fragments_[temp_id].push_back( std::move(temp_fragment) ) ;
	
      } // queue loop 
    }  // fragment loop
    
    
    //-------------------------------------------------
    // Check if some decisions are complete or timedout 
    // and create dedicated record
    //--------------------------------------------------

    for ( auto it = trigger_decisions_.begin() ;
	  it != trigger_decisions_.end() ; 
	  ++it ) {

      std::unique_ptr<dataformats::TriggerRecord> temp_record ; 
      
      // if ( current_time - it -> second.trigger_timestamp > max_time_difference_ ) {
      // 	ers::warning( TimedOutTriggerDecision( ERS_HERE, it -> second, current_time ) ) ;
      // 	temp_record = BuildTriggerRecord( it -> first ) ; 
      // }
      
      auto frag_it = fragments_.find( it -> first ) ;
      
      if ( frag_it != fragments.end() ) {
	
	if ( frag_it -> second.size() == it -> second.components.size() ) {
	  temp_record = BuildTriggerRecord( it -> first ) ; 
	}
      } 
    
      if ( temp_record.get() ) {
	
	try {
	  record_sink.push( std::move(temp_record), trigger_decision_timeout_ );
	} catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
	  std::ostringstream oss_warn;
	  oss_warn << "push to output queue \"" << get_name() << "\"";
	  ers::warning(
		       dunedaq::appfwk::QueueTimeoutExpired( ERS_HERE,
							     record_sink.get_name(),
							     oss_warn.str(),
							     std::chrono::duration_cast<std::chrono::milliseconds>(trigger_decision_timeout_).count()));
	}
	
      } // if there was a record to be send
    
    } // decision loop for complete record
      
    
    //-------------------------------------------------
    // Check if some fragments are obsolete 
    //--------------------------------------------------
    
    for ( auto it = fragments_.begin() ;
	  it != fragments_.end() ; 
	  ++it ) {

      if ( current_time - it -> second.get_header().trigger_timestamp > max_time_difference_ ) {

	ers::error( RemovingFragment( ERS_HERE, it -> second.get_header() ) ) ;
	
	//it = trigger_decisions_.erase( it ) ;

	// note that if we reached this point it means there is no corresponding trigger decision for this id
	// otherwise we would have created a dedicated trigger record (though probably incomplete)
	// so there is no need to check the trigger decision book 
      }
      
    } // fragment loop

  }  // working loop

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, received Fake trigger decision messages for " << receivedCount
           << " triggers.";
  ers::info(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}  


dataformats::TriggerRecord * FragmentReceiver::BuildTriggerRecord( const TriggerId & id ) {

  dataformats::TriggerRecord * trig_rec_ptr = new dataformats::TriggerRecord() ;
  
  auto trig_dec_it = trigger_decisions_.find( id ) ;
  const dfmessages::TriggerDecision & trig_dec = trig_dec_it -> second ;

  trig_rec_ptr -> set_trigger_number( trig_dec.trigger_number );
  trig_rec_ptr -> set_run_number( trig_dec.run_number );
  trig_rec_ptr -> set_trigger_timestamp(trig_dec.trigger_timestamp);

  auto frags_it = fragments_.find( id ) ;
  auto & frags = frags_it -> second ;

  timestamp_t min_time = 0, max_time = 0 ;

  for ( unsigned int i = 0 ; i < frags.size() ; ++i ) {

    
  }
  

  trig_rec_ptr -> set_fragments(frag_ptr_vector);


  // create and fill trigger header
  
  // create trigger record
  
  // fill header and fragments
  
  // somehow derive the requests and add them as well
  
  // all the operations have to be done removing the memory from the maps
}

  
} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::FragmentReceiver)
