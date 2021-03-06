/**
 * @file TriggerRecordBuilder.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_TRIGGERRECORDBUILDER_HPP_
#define DFMODULES_PLUGINS_TRIGGERRECORDBUILDER_HPP_

#include "dfmodules/TriggerDecisionForwarder.hpp"
#include "dfmodules/triggerrecordbuilderinfo/InfoNljs.hpp"

#include "dataformats/Fragment.hpp"
#include "dataformats/TriggerRecord.hpp"
#include "dataformats/Types.hpp"
#include "dfmessages/DataRequest.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/Types.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSink.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"

#include <chrono>
#include <tuple>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace dunedaq {

namespace dfmodules {

/**
 * @brief TriggerId is a little class that defines a unique identifier for a
 * trigger decision/record It also provides an operator < to be used by map to
 * optimise bookkeeping
 */
struct TriggerId {

  TriggerId() = default;

  explicit TriggerId(const dfmessages::TriggerDecision& td, 
		     dataformats::sequence_number_t s = dataformats::TypeDefaults::s_invalid_sequence_number)
    : trigger_number(td.trigger_number)
    , sequence_number(s)
    , run_number(td.run_number)
  {
    ;
  }
  explicit TriggerId(dataformats::Fragment& f)
    : trigger_number(f.get_trigger_number())
    , sequence_number(f.get_sequence_number())
    , run_number(f.get_run_number())
  {
    ;
  }

  dataformats::trigger_number_t trigger_number;
  dataformats::sequence_number_t sequence_number;
  dataformats::run_number_t run_number;

  bool operator<(const TriggerId& other) const noexcept
  {
    return std::tuple(trigger_number, sequence_number, run_number) <
           std::tuple(other.trigger_number, other.sequence_number, other.run_number);
  }

  friend std::ostream& operator<<(std::ostream& out, const TriggerId& id) noexcept
  {
    out << id.trigger_number << '-' << id.sequence_number  << '/' << id.run_number;
    return out;
  }

  friend TraceStreamer& operator<<(TraceStreamer& out, const TriggerId& id) noexcept
  {
    return out << id.trigger_number << '-' << id.sequence_number << "/" << id.run_number;
  }

  friend std::istream& operator>>(std::istream& in, TriggerId& id)
  {
    char t1,t2;
    in >> id.trigger_number >> t1 >> id.sequence_number >> t2 >> id.run_number;
    return in;
  }
};

} // namespace dfmodules

/**
 * @brief Timed out Trigger Decision
 */
ERS_DECLARE_ISSUE(
    dfmodules,               ///< Namespace
    TimedOutTriggerDecision, ///< Issue class name
    "trigger id: " << trigger_id << " generate at: " << trigger_timestamp
                   << " timed out",               ///< Message
    ((dfmodules::TriggerId)trigger_id)            ///< Message parameters
    ((dataformats::timestamp_t)trigger_timestamp) ///< Message parameters

)

/**
 * @brief Unexpected fragment
 */
ERS_DECLARE_ISSUE(
    dfmodules,          ///< Namespace
    UnexpectedFragment, ///< Issue class name
    "Unexpected Fragment for triggerID " << trigger_id << ", type "
                                         << fragment_type << ", " << geo_id,
    ((dfmodules::TriggerId)trigger_id)            ///< Message parameters
    ((dataformats::fragment_type_t)fragment_type) ///< Message parameters
    ((dataformats::GeoID)geo_id)                  ///< Message parameters
)

/**
 * @brief Unknown GeoID
 */
ERS_DECLARE_ISSUE(dfmodules,    ///< Namespace
                  UnknownGeoID, ///< Issue class name
                  "Uknown GeoID: " << geo_id,
                  ((dataformats::GeoID)geo_id) ///< Message parameters
)

/**
 * @brief Duplicate trigger decision
 */
ERS_DECLARE_ISSUE(dfmodules,                 ///< Namespace
                  DuplicatedTriggerDecision, ///< Issue class name
                  "Duplicated trigger ID " << trigger_id,
                  ((dfmodules::TriggerId)trigger_id) ///< Message parameters
)

/**
 * @brief Invalid System Type
 */
ERS_DECLARE_ISSUE(dfmodules,         ///< Namespace
                  InvalidSystemType, ///< Issue class name
                  "Unknown system type " << type,
                  ((std::string)type) ///< Message parameters
)

namespace dfmodules {

/**
 * @brief TriggerRecordBuilder is the Module that collects Trigger
 TriggersDecisions, sends the corresponding data requests and collects Fragment
 to form a complete Trigger Record. The TR then sent out possibly to a write
 module
*/
class TriggerRecordBuilder : public dunedaq::appfwk::DAQModule {
public:
  /**
   * @brief TriggerRecordBuilder Constructor
   * @param name Instance name for this TriggerRecordBuilder instance
   */
  explicit TriggerRecordBuilder(const std::string &name);

  TriggerRecordBuilder(const TriggerRecordBuilder &) =
      delete; ///< TriggerRecordBuilder is not copy-constructible
  TriggerRecordBuilder &operator=(const TriggerRecordBuilder &) =
      delete; ///< TriggerRecordBuilder is not copy-assignable
  TriggerRecordBuilder(TriggerRecordBuilder &&) =
      delete; ///< TriggerRecordBuilder is not move-constructible
  TriggerRecordBuilder &operator=(TriggerRecordBuilder &&) =
      delete; ///< TriggerRecordBuilder is not move-assignable

  void init(const data_t &) override;
  void get_info(opmonlib::InfoCollector &ci, int level) override;

protected:
  using trigger_decision_source_t =
      dunedaq::appfwk::DAQSource<dfmessages::TriggerDecision>;
  using datareqsink_t = dunedaq::appfwk::DAQSink<dfmessages::DataRequest>;
  using datareqsinkmap_t =
      std::map<dataformats::GeoID, std::unique_ptr<datareqsink_t>>;

  using fragment_source_t =
      dunedaq::appfwk::DAQSource<std::unique_ptr<dataformats::Fragment>>;
  using fragment_sources_t = std::vector<std::unique_ptr<fragment_source_t>>;

  using trigger_record_ptr_t = std::unique_ptr<dataformats::TriggerRecord>;
  using trigger_record_sink_t = appfwk::DAQSink<trigger_record_ptr_t>;

  bool read_fragments(fragment_sources_t &, bool drain = false);

  trigger_record_ptr_t extract_trigger_record(const TriggerId &);
  // build_trigger_record will allocate memory and then orphan it to the caller
  // via the returned pointer Plese note that the method will destroy the memory
  // saved in the bookkeeping map

  unsigned int create_trigger_records_and_dispatch( const dfmessages::TriggerDecision&, 
						    datareqsinkmap_t&, 
						    std::atomic<bool>& running ) ;
  
  bool dispatch_data_requests(const dfmessages::DataRequest&, const dataformats::GeoID &,
			      datareqsinkmap_t&, std::atomic<bool>& running) const;

  bool send_trigger_record(const TriggerId &, trigger_record_sink_t &,
                           std::atomic<bool> &running);
  // this creates a trigger record and send it
  
  bool check_stale_requests(trigger_record_sink_t &,
                            std::atomic<bool> &running);
  // it returns true when there are changes in the book = a TR timed out
  
private:
  // Commands
  void do_conf(const data_t &);
  void do_start(const data_t &);
  void do_stop(const data_t &);

  // Threading
  dunedaq::appfwk::ThreadHelper m_thread;
  void do_work(std::atomic<bool> &);

  // Configuration
  // size_t m_sleep_msec_while_running;
  std::chrono::milliseconds m_queue_timeout;
  std::chrono::milliseconds m_loop_sleep;

  // Input Queues
  std::string m_trigger_decision_source_name;
  std::vector<std::string> m_fragment_source_names;

  // Output queues
  std::string m_trigger_record_sink_name;
  std::map<dataformats::GeoID, std::string>
      m_map_geoid_queues; ///< Mappinng between GeoID and queues

  // bookeeping
  using clock_type = std::chrono::high_resolution_clock;
  std::map<TriggerId, std::pair<clock_type::time_point, trigger_record_ptr_t>>
      m_trigger_records;

  // Data request properties
  dataformats::timestamp_diff_t m_max_time_window ;

  // book related metrics
  using metric_counter_type =
      decltype(triggerrecordbuilderinfo::Info::pending_trigger_decisions);
  using metric_ratio_type =
      decltype(triggerrecordbuilderinfo::Info::average_millisecond_per_trigger);
  mutable std::atomic<metric_counter_type> m_trigger_decisions_counter = {
      0}; // currently
  mutable std::atomic<metric_counter_type> m_fragment_counter = {
      0}; // currently
  mutable std::atomic<metric_counter_type> m_pending_fragment_counter = {
      0}; // currently

  mutable std::atomic<metric_counter_type> m_timed_out_trigger_records = {
      0}; // in the run
  mutable std::atomic<metric_counter_type> m_unexpected_fragments = {
      0};                                                          // in the run
  mutable std::atomic<metric_counter_type> m_lost_fragments = {0}; // in the run
  mutable std::atomic<metric_counter_type> m_invalid_requests = {
      0}; // in the run
  mutable std::atomic<metric_counter_type> m_duplicated_trigger_ids = {
      0}; // in the run

  mutable std::atomic<metric_counter_type> m_completed_trigger_records = {
      0}; // in between calls
  mutable std::atomic<metric_counter_type> m_sleep_counter = {
      0}; // in between calls
  mutable std::atomic<metric_counter_type> m_loop_counter = {
      0};                                                    // in between calls
  mutable std::atomic<uint64_t> m_trigger_record_time = {0}; // in between calls
  mutable std::atomic<uint64_t> m_trigger_decision_width = {0}; // in between calls
  mutable std::atomic<uint64_t> m_data_request_width= {0};      // in between calls
  mutable std::atomic<metric_counter_type> m_received_trigger_decisions = {0}; // in between calls
  mutable std::atomic<metric_counter_type> m_generated_data_requests = {0}; // in between calls

  // time thresholds
  using duration_type = std::chrono::milliseconds;
  duration_type m_old_trigger_threshold;
  duration_type m_trigger_timeout;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_TRIGGERRECORDBUILDER_HPP_
