/**
 * @file FragmentAggregatorModule.hpp Module to dispatch data requests within an application, aggregate and send fragments
 * using the IOMManager
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_FRAGMENTAGGREGATOR_HPP_
#define DFMODULES_PLUGINS_FRAGMENTAGGREGATOR_HPP_

#include "daqdataformats/Fragment.hpp"
#include "daqdataformats/SourceID.hpp"
#include "dfmessages/DataRequest.hpp"

#include "appfwk/DAQModule.hpp"
#include "logging/Logging.hpp" // NOTE: if ISSUES ARE DECLARED BEFORE include logging/Logging.hpp, TLOG_DEBUG<<issue wont work.

#include "iomanager/Receiver.hpp"
#include "iomanager/Sender.hpp"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <tuple>

namespace dunedaq {
/**
 * @brief Unknown TRB
 */
ERS_DECLARE_ISSUE(dfmodules,                  ///< Namespace
                  UnknownFragmentDestination, ///< Issue class name
                  "Could not find a valid destination for sending Fragment with trigger number: "
                    << trg_num << " sequence number: " << seq_num << " from DLH " << src, ///< Message
                  ((uint64_t)trg_num)                                                     ///< Message parameters
                  ((uint16_t)seq_num)                                                     ///< Message parameters
                  ((daqdataformats::SourceID)src)                                         ///< Message parameters
)


ERS_DECLARE_ISSUE(dfmodules,                ///< Namespace
                  AbandonedFragment,        ///< Issue class name
                  "Fragment from " <<  source << " for trigger " << trigger << '-' << sequence << " of run " << run << " was dropped",
		  ((daqdataformats::run_number_t)run)
                  ((daqdataformats::trigger_number_t)trigger)
		  ((daqdataformats::sequence_number_t)sequence)              
		  ((daqdataformats::SourceID)source)
)

namespace dfmodules {

class FragmentAggregatorModule : public dunedaq::appfwk::DAQModule
{
public:
  explicit FragmentAggregatorModule(const std::string& name);

  FragmentAggregatorModule(const FragmentAggregatorModule&) = delete;
  FragmentAggregatorModule& operator=(const FragmentAggregatorModule&) = delete;
  FragmentAggregatorModule(FragmentAggregatorModule&&) = delete;
  FragmentAggregatorModule& operator=(FragmentAggregatorModule&&) = delete;

  void init(std::shared_ptr<appfwk::ConfigurationManager> mcfg) override;
  void generate_opmon_data() override;

private:
  // Commands
  void do_start(const nlohmann::json& obj);
  void do_stop(const nlohmann::json& obj);

  void process_data_request(dfmessages::DataRequest&);
  void process_fragment(std::unique_ptr<daqdataformats::Fragment>&);

  // Input and Output Connection namess
  std::string m_data_req_input;
  std::string m_fragment_input;
  std::map<int, std::string> m_producer_conn_ids;
  std::vector<std::string> m_trb_conn_ids;

  // Opmon
  using metric_counter_type = uint64_t;
  std::atomic<metric_counter_type> m_data_requests_received{ 0 };
  std::atomic<metric_counter_type> m_data_requests_processed{ 0 };
  std::atomic<metric_counter_type> m_data_requests_failed{ 0 };
  std::atomic<metric_counter_type> m_fragments_received{ 0 };
  std::atomic<metric_counter_type> m_fragments_processed{ 0 };
  std::atomic<metric_counter_type> m_fragments_failed{ 0 };
  std::atomic<metric_counter_type> m_fragments_empty{ 0 };
  std::atomic<metric_counter_type> m_fragments_incomplete{ 0 };
  std::atomic<metric_counter_type> m_fragments_invalid{ 0 };
  std::atomic<metric_counter_type> m_fragments_time_average_us{ 0 };
  std::atomic<metric_counter_type> m_fragments_time_min_us{ 0 };
  std::atomic<metric_counter_type> m_fragments_time_max_us{ 0 };
  std::atomic<metric_counter_type> m_data_requests_time_average_us{ 0 };
  std::atomic<metric_counter_type> m_data_requests_time_min_us{ 0 };
  std::atomic<metric_counter_type> m_data_requests_time_max_us{ 0 };

  // TRB tracking
  std::map<std::tuple<dfmessages::trigger_number_t, dfmessages::sequence_number_t, daqdataformats::SourceID>,
           std::string>
    m_data_req_map;
  std::mutex m_mutex;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_FRAGMENTAGGREGATOR_HPP_
