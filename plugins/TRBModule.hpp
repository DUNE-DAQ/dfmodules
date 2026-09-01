/**
 * @file TRBModule.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_TRIGGERRECORDBUILDER_HPP_
#define DFMODULES_PLUGINS_TRIGGERRECORDBUILDER_HPP_

#include "appmodel/ReadoutApplication.hpp"
#include "appmodel/SmartDaqApplication.hpp"
#include "appmodel/TRBConf.hpp"
#include "daqdataformats/Fragment.hpp"
#include "daqdataformats/SourceID.hpp"
#include "daqdataformats/TriggerRecord.hpp"
#include "daqdataformats/Types.hpp"
#include "dfmessages/DataRequest.hpp"
#include "dfmessages/TRBCompletion.hpp"
#include "dfmessages/TRMonRequest.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/TriggerId.hpp"
#include "dfmessages/Types.hpp"

#include "appfwk/DAQModule.hpp"
#include "iomanager/Receiver.hpp"
#include "iomanager/Sender.hpp"
#include "logging/Logging.hpp" // NOTE: if ISSUES ARE DECLARED BEFORE include logging/Logging.hpp, TLOG_DEBUG<<issue wont work.
#include "utilities/WorkerThread.hpp"

#include "dfmodules/opmon/TRBModule.pb.h"

#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace dunedaq {

/**
 * @brief Unexpected trigger decision
 */
ERS_DECLARE_ISSUE(dfmodules,                 ///< Namespace
                  UnexpectedTriggerDecision, ///< Issue class name
                  "Unexpected Trigger Decisions: " << trigger << '/' << decision_run << " while in run " << current_run,
                  ((daqdataformats::trigger_number_t)trigger)  ///< Message parameters
                  ((daqdataformats::run_number_t)decision_run) ///< Message parameters
                  ((daqdataformats::run_number_t)current_run)  ///< Message parameters
)

/**
 * @brief Timed out Trigger Decision
 */
ERS_DECLARE_ISSUE(dfmodules,               ///< Namespace
                  TimedOutTriggerDecision, ///< Issue class name
                  "trigger id: " << trigger_id << " generate at: " << trigger_timestamp << " timed out", ///< Message
                  ((dfmessages::TriggerId)trigger_id)              ///< Message parameters
                  ((daqdataformats::timestamp_t)trigger_timestamp) ///< Message parameters
)

/**
 * @brief Unexpected fragment
 */
ERS_DECLARE_ISSUE(dfmodules,          ///< Namespace
                  UnexpectedFragment, ///< Issue class name
                  "Unexpected Fragment for triggerID " << trigger_id << ", sequence " << sequence_number << ", type "
                                                       << fragment_type << ", " << source_id,
                  ((dfmessages::TriggerId)trigger_id)              ///< Message parameters
                  ((dfmessages::sequence_number_t)sequence_number) ///< Message parameters
                  ((daqdataformats::fragment_type_t)fragment_type) ///< Message parameters
                  ((daqdataformats::SourceID)source_id)            ///< Message parameters
)

/**
 * @brief Duplicate trigger decision
 */
ERS_DECLARE_ISSUE(dfmodules,                 ///< Namespace
                  DuplicatedTriggerDecision, ///< Issue class name
                  "Duplicated trigger ID " << trigger_id << ", sequence " << sequence_number,
                  ((dfmessages::TriggerId)trigger_id) ///< Message parameters
                  ((dfmessages::sequence_number_t)sequence_number) ///< Message parameters
)

/**
 * @brief Abandoned TR
 */
ERS_DECLARE_ISSUE(dfmodules,                ///< Namespace
                  AbandonedTriggerRecord, ///< Issue class name
                  "trigger ID " << trigger_id << " could not be sent to writing and it's lost",
                  ((dfmessages::TriggerId)trigger_id) ///< Message parameters
                  ((dfmessages::sequence_number_t)sequence_number) ///< Message parameters
)

/**
 * @brief Incomplete TR
 */
ERS_DECLARE_ISSUE(dfmodules,               ///< Namespace
                  IncompleteTriggerRecord, ///< Issue class name
                  "sending incomplete TriggerRecord downstream "
                    << optional_stop_time_phrase << " (trigger/run_number=" << id << " (seq " << sequence_number
                    << "), " << num_frags_present << " of " << num_components_requested << " fragments included)",
                  ((std::string)optional_stop_time_phrase)         ///< Message parameters
                  ((dfmessages::TriggerId)id)                      ///< Message parameters
                  ((dfmessages::sequence_number_t)sequence_number) ///< Message parameters
                  ((int)num_frags_present)                         ///< Message parameters
                  ((int)num_components_requested)                  ///< Message parameters
)

/**
 * @brief Missing connection ID
 */
ERS_DECLARE_ISSUE(dfmodules,           ///< Namespace
                  MissingConnectionID, ///< Issue class name
                  "No connection ID was found for connection name \""
                    << conn_name << "\" in the conn_ref list that was provided at 'init' time.",
                  ((std::string)conn_name) ///< Message parameters
)

namespace dfmodules {

/**
 * @brief TRBModule is the Module that collects Trigger
 TriggersDecisions, sends the corresponding data requests and collects Fragment
 to form a complete Trigger Record. The TR then sent out possibly to a write
 module
*/
class TRBModule : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief TRBModule Constructor
   * @param name Instance name for this TRBModule instance
   */
  explicit TRBModule(const std::string& name);

  TRBModule(const TRBModule&) = delete;            ///< TRBModule is not copy-constructible
  TRBModule& operator=(const TRBModule&) = delete; ///< TRBModule is not copy-assignable
  TRBModule(TRBModule&&) = delete;                 ///< TRBModule is not move-constructible
  TRBModule& operator=(TRBModule&&) = delete;      ///< TRBModule is not move-assignable

  void init(std::shared_ptr<appfwk::ConfigurationManager> mcfg) override;

  void generate_opmon_data() override;

protected:
    struct TriggerRecordId
    {
      dfmessages::TriggerId trigger_id;
      dfmessages::sequence_number_t sequence_number;
      bool operator<(const TriggerRecordId& other) const
      {
        return std::tie(trigger_id, sequence_number) < std::tie(other.trigger_id, other.sequence_number);
      }

      friend std::ostream& operator<<(std::ostream& out, const TriggerRecordId& id) noexcept
      {
        out << id.trigger_id.trigger_number << "." << id.sequence_number << "/" << id.trigger_id.run_number;
        return out;
      }
  };

  using trigger_decision_receiver_t = iomanager::ReceiverConcept<dfmessages::TriggerDecision>;
  using data_req_sender_t = iomanager::SenderConcept<dfmessages::DataRequest>;
  using fragment_receiver_t = iomanager::ReceiverConcept<std::unique_ptr<daqdataformats::Fragment>>;

  using trigger_record_ptr_t = std::unique_ptr<daqdataformats::TriggerRecord>;
  using trigger_record_sender_t = iomanager::SenderConcept<trigger_record_ptr_t>;
  using trb_complete_sender_t = iomanager::SenderConcept<dfmessages::TRBCompletion>;

  void trigger_decision_callback(dfmessages::TriggerDecision& td);
  void fragments_callback(std::unique_ptr<daqdataformats::Fragment>& frag);

  trigger_record_ptr_t extract_trigger_record(const TriggerRecordId&);
  // build_trigger_record will allocate memory and then orphan it to the caller
  // via the returned pointer Plese note that the method will destroy the memory
  // saved in the bookkeeping map

  unsigned int create_trigger_records_and_dispatch(const dfmessages::TriggerDecision&);

  bool dispatch_data_requests(dfmessages::DataRequest, const daqdataformats::SourceID&);

  bool send_trigger_record(const TriggerRecordId &);
  // this creates a trigger record and send it

  bool check_stale_requests();
  // it returns true when there are changes in the book = a TR timed out

  void flush_trigger_records();

private:
  // Commands
  void do_conf(const CommandData_t&);
  void do_scrap(const CommandData_t&);
  void do_start(const CommandData_t&);
  void do_stop(const CommandData_t&);

  // Monitoring callback
  void tr_requested(const dfmessages::TRMonRequest&);

  // Threading
  std::atomic<bool> m_stop_requested;

  // Configuration
  const appmodel::TRBConf* m_trb_conf;
  std::chrono::milliseconds m_tr_queue_timeout;
  std::chrono::milliseconds m_trb_complete_timeout;
  std::chrono::milliseconds m_dreq_queue_timeout;
  std::string m_reply_connection;
  size_t m_max_open_trigger_records;
  daqdataformats::SourceID m_this_trb_source_id;

  // Input Connections
  std::shared_ptr<trigger_decision_receiver_t> m_trigger_decision_input;
  std::shared_ptr<fragment_receiver_t> m_fragment_input;

  // Output connections
  std::shared_ptr<trigger_record_sender_t> m_trigger_record_output;
  std::shared_ptr<trb_complete_sender_t> m_trb_complete_output;
  mutable std::mutex m_map_sourceid_connections_mutex;
  std::map<daqdataformats::SourceID, std::shared_ptr<data_req_sender_t>>
    m_map_sourceid_connections; ///< Mappinng between SourceID and connections

  // bookeeping
  using clock_type = std::chrono::steady_clock;
  std::mutex m_trigger_records_mutex;
  clock_type::time_point m_last_bookkeeping{};
  std::map<TriggerRecordId, std::pair<clock_type::time_point, trigger_record_ptr_t>>
    m_trigger_records;
  std::condition_variable m_open_trigger_record_cv;

  // Data request properties
  daqdataformats::timestamp_diff_t m_max_sequence_length;

  // Run information
  std::atomic<daqdataformats::run_number_t> m_run_number{ 0 };

  // Monitoring related variables
  std::mutex m_mon_mutex;
  std::shared_ptr<iomanager::ReceiverConcept<dfmessages::TRMonRequest>> m_mon_receiver;
  std::list<dfmessages::TRMonRequest> m_mon_requests;

  // book related metrics
  using metric_counter_type = uint64_t; // decltype(triggerrecordbuilderinfo::Info::pending_trigger_decisions);
  mutable std::atomic<metric_counter_type> m_trigger_decisions_counter = { 0 }; // currently
  mutable std::atomic<metric_counter_type> m_fragment_counter = { 0 };          // currently
  mutable std::atomic<metric_counter_type> m_pending_fragment_counter = { 0 };  // currently

  mutable std::atomic<metric_counter_type> m_timed_out_trigger_records = { 0 };    // in the run
  mutable std::atomic<metric_counter_type> m_unexpected_fragments = { 0 };         // in the run
  mutable std::atomic<metric_counter_type> m_unexpected_trigger_decisions = { 0 }; // in the run
  mutable std::atomic<metric_counter_type> m_lost_fragments = { 0 };               // in the run
  mutable std::atomic<metric_counter_type> m_invalid_requests = { 0 };             // in the run
  mutable std::atomic<metric_counter_type> m_duplicated_trigger_ids = { 0 };       // in the run
  mutable std::atomic<metric_counter_type> m_abandoned_trigger_records = { 0 };    // in the run

  mutable std::atomic<metric_counter_type> m_received_trigger_decisions = { 0 }; // in between calls
  mutable std::atomic<metric_counter_type> m_received_fragments = { 0 };         // in between calls
  mutable std::atomic<metric_counter_type> m_generated_trigger_records = { 0 };  // in between calls
  mutable std::atomic<metric_counter_type> m_generated_data_requests = { 0 };    // in between calls
  mutable std::atomic<metric_counter_type> m_data_waiting_time = { 0 };          // in between calls
  mutable std::atomic<metric_counter_type> m_trigger_decision_width = { 0 };     // in between calls
  mutable std::atomic<metric_counter_type> m_data_request_width = { 0 };         // in between calls
  mutable std::atomic<metric_counter_type> m_td_processing_us = { 0 };           // in between calls
  mutable std::atomic<metric_counter_type> m_fragment_processing_us = { 0 };     // in between calls

  mutable std::atomic<metric_counter_type> m_trmon_request_counter = { 0 };
  mutable std::atomic<metric_counter_type> m_trmon_sent_counter = { 0 };

  // time thresholds
  using duration_type = std::chrono::microseconds;
  duration_type m_old_trigger_threshold;
  duration_type m_trigger_timeout;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_TRIGGERRECORDBUILDER_HPP_
