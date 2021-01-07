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
#include "appfwk/cmd/Nljs.hpp"
#include "dfmodules/fragmentreceiver/Structs.hpp"
#include "dfmodules/fragmentreceiver/Nljs.hpp"

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

  FragmentReceiver::FragmentReceiver(const std::string& name) : 
    dunedaq::appfwk::DAQModule(name),
    thread_(std::bind(&FragmentReceiver::do_work, this, std::placeholders::_1)),
    trigger_decision_timeout_(100),
    fragment_timeout_(100) {
    
    register_command("conf", &FragmentReceiver::do_conf);
    register_command("start", &FragmentReceiver::do_start);
    register_command("stop", &FragmentReceiver::do_stop);
  }
  
  void
  FragmentReceiver::init(const data_t& init_data) {
    
    TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
    
    //--------------------------------
    // Get single queues
    //---------------------------------
    
    auto qi = appfwk::qindex(init_data, { "trigger_decision_input_queue", "trigger_record_output_queue" } );
    // data request input queue
    try {
      auto temp_info = qi["trigger_decision_input_queue"] ;
      std::string temp_name = temp_info.inst ;
      trigger_decision_source_t test( temp_name ) ;
      trigger_decision_source_name_ = temp_name ;
    } catch (const ers::Issue& excpt) {
      throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_decision_input_queue", excpt);
    }
    
    
    // trigger record output
    try {
      auto temp_info = qi["trigger_record_output_queue"] ;
      std::string temp_name = temp_info.inst ;
      trigger_record_sink_t test( temp_name ) ;
      trigger_record_sink_name_ = temp_name ;
    } catch (const ers::Issue& excpt) {
      throw InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_record_output_queue", excpt);
    }
    
    
    //----------------------
    // Get dynamic queues
    //----------------------
    
    // set names for the 
    auto ini = init_data.get<appfwk::cmd::ModInit>() ;
    
    // get the names for the fragment queues
    for (const auto& qitem : ini.qinfos) {
      if (qitem.name.rfind("data_fragment_") == 0) {
	try {
	  std::string temp_name = qitem.inst ;
	  fragment_source_t test ( temp_name ) ;
	  fragment_source_names_.push_back( temp_name ) ;
	} catch (const ers::Issue& excpt) {
	  throw InvalidQueueFatalError(ERS_HERE, get_name(), qitem.name, excpt);
	}
      }
    }
    
    TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
  } 
  
  
  void
  FragmentReceiver::do_conf(const data_t& payload )
  {
    TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";
    
    fragmentreceiver::ConfParams parsed_conf = payload.get<fragmentreceiver::ConfParams>() ;

    decision_loop_cnt_ = parsed_conf.decision_loop_counter ; 
    fragment_loop_cnt_ = parsed_conf.fragment_loop_counter == 0 ? 
      fragment_source_names_.size() : parsed_conf.fragment_loop_counter ;
    
    max_time_difference_  = parsed_conf.max_timestamp_diff ;
    
    trigger_decision_timeout_ = std::chrono::milliseconds( parsed_conf.general_queue_timeout ) ;
    auto scaling_factor = fragment_loop_cnt_ * fragment_source_names_.size() ;
    if ( scaling_factor <= 0 ) scaling_factor = 1 ;
    fragment_timeout_ = std::chrono::milliseconds( parsed_conf.general_queue_timeout / 
						   scaling_factor ) ;   
    
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
      frag_sources.push_back( std::unique_ptr<fragment_source_t> ( new fragment_source_t( fragment_source_names_[i] ) ) ); 
    }
    
    dataformats::timestamp_t current_time = 0 ; 
  
    // temp memory allocations
    dfmessages::TriggerDecision temp_dec ;
    std::unique_ptr<dataformats::Fragment> temp_fragment ; 

    while (running_flag.load()) {

      std::ostringstream message ;
      message << "Bookeeping status: "
	      << trigger_decisions_.size() << " decisions and " 
	      << fragments_.size() << " Fragment stashes" 
	      << std::endl 
	      << "Trigger Decisions" << std::endl ;
      
      for ( const auto & d : trigger_decisions_ ) {
	message << "\t" << d.first << " with " << d.second.components.size() << " components " << std::endl ;
      }
      message << "Fragments" << std::endl ;
      for ( const auto & f : fragments_ ) {
	message << "\t" << f.first << " with " << f.second.size() << " fragments " << std::endl ;
      }
     
      ers::info(ProgressUpdate(ERS_HERE, get_name(), message.str()));


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

	TriggerId temp_id( temp_dec ) ;
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
	
	  TriggerId temp_id( *temp_fragment ) ;
	  fragments_[temp_id].push_back( std::move(temp_fragment) ) ;
	
	} // queue loop 
      }  // fragment loop
    
    
      TLOG(TLVL_WORK_STEPS) << "Decision size: " << trigger_decisions_.size() ;
      TLOG(TLVL_WORK_STEPS) << "Frag size: " << fragments_.size() ;

      //-------------------------------------------------
      // Check if some decisions are complete or timedout 
      // and create dedicated record
      //--------------------------------------------------

      for ( auto it = trigger_decisions_.begin() ;
	    it != trigger_decisions_.end() ; ) {

	std::unique_ptr<dataformats::TriggerRecord> temp_record ; 
      
	// if ( current_time - it -> second.trigger_timestamp > max_time_difference_ ) {
	// 	ers::warning( TimedOutTriggerDecision( ERS_HERE, it -> second, current_time ) ) ;
	// 	temp_record = BuildTriggerRecord( it -> first ) ; 
	// }
      
	auto frag_it = fragments_.find( it -> first ) ;
      
	if ( frag_it != fragments_.end() ) {
	
	  if ( frag_it -> second.size() >= it -> second.components.size() ) {
	    temp_record.reset( BuildTriggerRecord( it -> first ) ) ; 
	  }
	  else {
	    std::ostringstream message ;
	    message << "Trigger decision stauts: " 
		    << frag_it -> second.size() << " / " << it -> second.components.size() << "Fragments" ;
	    ers::error(ProgressUpdate(ERS_HERE, get_name(), message.str()));
	  }

	} 
    
	if ( temp_record.get() ) {

	  bool wasSentSuccessfully = false;
	  while( !wasSentSuccessfully ) {
	    try {
	      record_sink.push( std::move(temp_record), trigger_decision_timeout_ );
	      wasSentSuccessfully = true ;
	    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
	      std::ostringstream oss_warn;
	      oss_warn << "push to output queue \"" << get_name() << "\"";
	      ers::warning(
			   dunedaq::appfwk::QueueTimeoutExpired( ERS_HERE,
								 record_sink.get_name(),
								 oss_warn.str(),
								 std::chrono::duration_cast<std::chrono::milliseconds>(trigger_decision_timeout_).count()));
	    }
	  }  // push while loop
	
          // 07-Jan-2021, KAB: the following "break" statement is needed to get things to work.
          // Without it, the code hangs on the ++it; in the "else" block below in the next iteration
          // of the loop.  I presume that this is because the trigger_decisions_ map has been
          // modified and the iterator has somehow been invalidated.
          // This break statement worries me a little bit because I don't know how it might
          // interact with a queue push timeout above.  But, for now, it is definitely needed.
          break;
	} // if there was a record to be send
	else {
	  ++it ;
	}
	
      } // decision loop for complete record
      
    
      //-------------------------------------------------
      // Check if some fragments are obsolete 
      //--------------------------------------------------
    
      for ( auto it = fragments_.begin() ;
	    it != fragments_.end() ; 
	    ++it ) {

	for ( auto frag_it = it -> second.begin() ;
	      frag_it != it-> second.end() ;
	      ++ frag_it ) {
	  
	  if ( current_time > max_time_difference_  + (*frag_it) -> get_trigger_timestamp() ) {

	    ers::error( FragmentObsolete( ERS_HERE, 
					  (*frag_it) -> get_trigger_number(),  
					  (*frag_it) -> get_fragment_type(),
					  (*frag_it) -> get_trigger_timestamp(),
					  current_time ) ) ;
	    //it = trigger_decisions_.erase( it ) ;

	    // note that if we reached this point it means there is no corresponding trigger decision for this id
	  // otherwise we would have created a dedicated trigger record (though probably incomplete)
	  // so there is no need to check the trigger decision book 
	  }
	} // vector loop
      } // fragment loop

    }  // working loop

    std::ostringstream oss_summ;
    oss_summ << ": Exiting the do_work() method, " 
	     << trigger_decisions_.size() << " reminaing Trigger Decision and " 
	     << fragments_.size() << " remaining fragment stashes" ;
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

    while( frags.size() > 0 ) {
      trig_rec_ptr -> add_fragment( std::move( frags.back() ) ) ;
      frags.pop_back() ;
    }

    trigger_decisions_.erase( trig_dec_it ) ;
    fragments_.erase( frags_it ) ;

    return trig_rec_ptr ;
  }

  
} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::FragmentReceiver)
