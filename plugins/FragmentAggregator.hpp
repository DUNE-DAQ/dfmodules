/**
 * @file FragmentAggregator.hpp Module to dispatch data requests within an application, aggregate and send fragments
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

namespace dfmodules {

class FragmentAggregator : public dunedaq::appfwk::DAQModule
{
public:
  explicit FragmentAggregator(const std::string& name);

  FragmentAggregator(const FragmentAggregator&) = delete;
  FragmentAggregator& operator=(const FragmentAggregator&) = delete;
  FragmentAggregator(FragmentAggregator&&) = delete;
  FragmentAggregator& operator=(FragmentAggregator&&) = delete;

  void init(std::shared_ptr<appfwk::ModuleConfiguration> mcfg) override;
  void get_info(opmonlib::InfoCollector& ci, int level) override;

private:
  // Commands
  void do_start(const nlohmann::json& obj);
  void do_stop(const nlohmann::json& obj);

  void process_data_request(dfmessages::DataRequest&);
  void process_fragment(std::unique_ptr<daqdataformats::Fragment>&);

  // Input Connection namess
  std::string m_data_req_input;
  std::string m_fragment_input;
  std::map<std::string, std::string> m_producer_conn_ref_map;

  // Stats
  std::atomic<int> m_packets_processed{ 0 };

  // TRB tracking
  std::map<std::tuple<dfmessages::trigger_number_t, dfmessages::sequence_number_t, daqdataformats::SourceID>,
           std::string>
    m_data_req_map;
  std::mutex m_mutex;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_FRAGMENTAGGREGATOR_HPP_
