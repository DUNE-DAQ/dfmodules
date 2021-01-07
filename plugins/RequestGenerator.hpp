/**
 * @file RequestGenerator.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_REQUESTGENERATOR_HPP_
#define DFMODULES_SRC_REQUESTGENERATOR_HPP_

#include "dfmodules/TriggerDecisionForwarder.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSink.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"
#include "dfmessages/DataRequest.hpp"
#include "dfmessages/TriggerDecision.hpp"

#include <memory>
#include <string>
#include <vector>
#include <map>

namespace dunedaq {
  using apatype = decltype(dataformats::GeoID::apa_number) ;
  using linktype = decltype(dataformats::GeoID::link_number) ;
			  
  ERS_DECLARE_ISSUE(dfmodules,                 ///< Namespace
                    UnknownGeoID,  ///< Issue class name
                    "trigger number " << trigger_number
		    << " of run: " << run_number
		    << " of APA: " << apa
		    << " of Link: " << link, 
		    ((dataformats::trigger_number_t)trigger_number)  ///< Message parameters 
		    ((dataformats::run_number_t)run_number)          ///< Message parameters 
		    ((apatype)apa)    ///< Message parameters 
		    ((linktype)link)    ///< Message parameters 
		    )

namespace dfmodules {

/**
 * @brief RequestGenerator is simply an example
 */
class RequestGenerator : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief RequestGenerator Constructor
   * @param name Instance name for this RequestGenerator instance
   */
  explicit RequestGenerator(const std::string& name);

  RequestGenerator(const RequestGenerator&) = delete;            ///< RequestGenerator is not copy-constructible
  RequestGenerator& operator=(const RequestGenerator&) = delete; ///< RequestGenerator is not copy-assignable
  RequestGenerator(RequestGenerator&&) = delete;                 ///< RequestGenerator is not move-constructible
  RequestGenerator& operator=(RequestGenerator&&) = delete;      ///< RequestGenerator is not move-assignable

  void init(const data_t&) override;

private:
  // Commands
  //  void do_conf(const data_t&);
  void do_conf(const nlohmann::json& obj);
  void do_start(const data_t&);
  void do_stop(const data_t&);

  // Threading
  dunedaq::appfwk::ThreadHelper thread_;
  void do_work(std::atomic<bool>&);

  // Configuration
  // size_t sleepMsecWhileRunning_;
  std::chrono::milliseconds queueTimeout_;

  // Queue(s)
  using trigdecsource_t = dunedaq::appfwk::DAQSource<dfmessages::TriggerDecision>;
  std::unique_ptr<trigdecsource_t> triggerDecisionInputQueue_;
  using trigdecsink_t = dunedaq::appfwk::DAQSink<dfmessages::TriggerDecision>;
  std::unique_ptr<trigdecsink_t> triggerDecisionOutputQueue_;
  using datareqsink_t = dunedaq::appfwk::DAQSink<dfmessages::DataRequest>;
  //  std::vector<std::unique_ptr<datareqsink_t>> dataRequestOutputQueues_;
  std::map<dataformats::GeoID, std::string> m_map_geoid_queues;
  // Worker(s)
  std::unique_ptr<TriggerDecisionForwarder> trigger_decision_forwarder_;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_REQUESTGENERATOR_HPP_
