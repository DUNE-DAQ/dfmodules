/**
 * @file FragmentReceiver.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_FRAGMENTRECEIVER_HPP_
#define DFMODULES_PLUGINS_FRAGMENTRECEIVER_HPP_

#include "dataformats/Fragment.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/Types.hpp"
#include "dataformats/Types.hpp"
#include "dataformats/TriggerRecord.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSink.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"

#include <memory>
#include <string>
#include <vector>
#include <map>

namespace dunedaq {

  /**
   * @brief Timed out Trigger Decision
   */
  ERS_DECLARE_ISSUE(dfmodules,                 ///< Namespace
                    TimedOutTriggerDecision,  ///< Issue class name
                    "trigger number " << decision.trigger_number
		    << " of run: " << decision.run_number
		    << " generate at: " << decision.trigger_timestamp
		    << " too late for: " << present_time,     ///< Message
                    ((dfmessages::TriggerDecision)decision)   ///< Message parameters
		    ((dataformats::timestamp_t)present_time)  ///< Message parameters 
		    )


  /**
   * @brief Removing fragment
   */
  ERS_DECLARE_ISSUE(dfmodules,         ///< Namespace
                    RemovingFragment,  ///< Issue class name
                    "trigger_number: " << header.trigger_number
		    << " type: " << header.fragment_type
		    << " GEO Id: " << header.link_ID,         ///< Message
                    ((dataformats::FragmentHeader)header)     ///< Message parameters
		    )

  
  namespace dfmodules {

    
    /**
     * @brief TriggerId is a little class that defines a unique identifier for a trigger decision/record
     * It also provides an operator < to be used by map to optimise bookkeeping 
     */
    struct TriggerId {
      
      TriggerId( const dfmessages::TriggerDecision & td ) : trigger_number(td.trigger_number), 
							    run_number(td.run_number) { ; } 
      TriggerId( dataformats::Fragment & f ) : trigger_number(f.get_trigger_number()), 
		                               run_number(f.get_run_number()) { ; } 
      
      
      dataformats::trigger_number_t trigger_number;
      dataformats::run_number_t run_number;
      
      bool operator < ( const TriggerId & other ) noexcept { 
	return  run_number == other.run_number ? 
	  trigger_number < other.trigger_number : 
	  run_number < other.run_number ; 
      }
    };
  
    

    
    /**
     * @brief FragmentReceiver is the Module that collects Fragment from the Upstream DAQ Modules, it checks 
     *        if they corresponds to a Trigger Decision, and once a Trigger Decision has all its fragments, 
     *        it sends the data (The complete Trigger Record) to a writer
     */
    class FragmentReceiver : public dunedaq::appfwk::DAQModule
    {
    public:
      /**
       * @brief FragmentReceiver Constructor
       * @param name Instance name for this FragmentReceiver instance
       */
      explicit FragmentReceiver(const std::string& name);

      FragmentReceiver(const FragmentReceiver&) = delete;            ///< FragmentReceiver is not copy-constructible
      FragmentReceiver& operator=(const FragmentReceiver&) = delete; ///< FragmentReceiver is not copy-assignable
      FragmentReceiver(FragmentReceiver&&) = delete;                 ///< FragmentReceiver is not move-constructible
      FragmentReceiver& operator=(FragmentReceiver&&) = delete;      ///< FragmentReceiver is not move-assignable

      void init(const data_t&) override;


    protected:
      dataformats::TriggerRecord * BuildTriggerRecord( const TriggerId & ) ;
      // Plese note that the method will destroy the memory saved in the bookkeeping map
  
    private:
      // Commands
      void do_conf(const data_t&);
      void do_start(const data_t&);
      void do_stop(const data_t&);

      // Threading
      dunedaq::appfwk::ThreadHelper thread_;
      void do_work(std::atomic<bool>&);

      // Configuration
      // size_t sleepMsecWhileRunning_;
      std::chrono::milliseconds trigger_decision_timeout_;
      std::chrono::milliseconds fragment_timeout_;

      // Input Queues
      using trigger_decision_source_t = dunedaq::appfwk::DAQSource<dfmessages::TriggerDecision> ;
      using fragment_source_t = dunedaq::appfwk::DAQSource<std::unique_ptr<dataformats::Fragment>> ;

      std::string trigger_decision_source_name_ ;
      std::vector<std::string> fragment_source_names_ ; 
  
      // Output queues
      using trigger_record_sink_t = appfwk::DAQSink<std::unique_ptr<dataformats::TriggerRecord>> ;
      std::string trigger_record_sink_name_ ;

      // loop configurations
      unsigned int decision_loop_cnt_ ;
      unsigned int fragment_loop_cnt_  ;
      dataformats::timestamp_diff_t max_time_difference_ ; 

      // bookeeping
      std::map<TriggerId, std::vector<std::unique_ptr<dataformats::Fragment>>> fragments_ ; 
      std::map<TriggerId, dfmessages::TriggerDecision> trigger_decisions_ ;

    };
  } // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_FRAGMENTRECEIVER_HPP_
