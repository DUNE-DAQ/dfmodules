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
#include "dataformats/TriggerRecord.hpp"
#include "dataformats/Types.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/Types.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSink.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq {

/**
 * @brief Timed out Trigger Decision
 */
ERS_DECLARE_ISSUE(dfmodules,               ///< Namespace
                  TimedOutTriggerDecision, ///< Issue class name
                  "trigger number " << trigger_number << " of run: " << run_number << " generate at: "
                                    << trigger_timestamp << " too late for: " << present_time, ///< Message
                  ((dataformats::trigger_number_t)trigger_number)                              ///< Message parameters
                  ((dataformats::run_number_t)run_number)                                      ///< Message parameters
                  ((dataformats::timestamp_t)trigger_timestamp)                                ///< Message parameters
                  ((dataformats::timestamp_t)present_time)                                     ///< Message parameters
)

/**
 * @brief Removing fragment
 */
ERS_DECLARE_ISSUE(dfmodules,        ///< Namespace
                  FragmentObsolete, ///< Issue class name
                  "Fragment obsolete - trigger_number: " << trigger_number << " type: " << fragment_type
                                                         << " with timestamp: " << trigger_timestamp
                                                         << " and present time is " << present_time, ///< Message
                  ((dataformats::trigger_number_t)trigger_number) ///< Message parameters
                  ((dataformats::fragment_type_t)fragment_type)   ///< Message parameters
                  ((dataformats::timestamp_t)trigger_timestamp)   ///< Message parameters
                  ((dataformats::timestamp_t)present_time)        ///< Message parameters
)

namespace dfmodules {

/**
 * @brief TriggerId is a little class that defines a unique identifier for a trigger decision/record
 * It also provides an operator < to be used by map to optimise bookkeeping
 */
struct TriggerId
{

  explicit TriggerId(const dfmessages::TriggerDecision& td)
    : trigger_number(td.trigger_number)
    , run_number(td.run_number)
  {
    ;
  }
  explicit TriggerId(dataformats::Fragment& f)
    : trigger_number(f.get_trigger_number())
    , run_number(f.get_run_number())
  {
    ;
  }

  dataformats::trigger_number_t trigger_number;
  dataformats::run_number_t run_number;

  bool operator<(const TriggerId& other) const noexcept
  {
    return run_number == other.run_number ? trigger_number < other.trigger_number : run_number < other.run_number;
  }

  friend std::ostream& operator<<(std::ostream& out, const TriggerId& id)
  {
    out << id.trigger_number << '/' << id.run_number;
    return out;
  }

  friend TraceStreamer& operator<<(TraceStreamer& out, const TriggerId& id)
  {
    return out << id.trigger_number << "/" << id.run_number;
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
  
  using trigger_decision_source_t = dunedaq::appfwk::DAQSource<dfmessages::TriggerDecision>;
  using fragment_source_t = dunedaq::appfwk::DAQSource<std::unique_ptr<dataformats::Fragment>>;
  using fragment_sources_t = std::vector<std::unique_ptr<fragment_source_t>>;
  using trigger_record_sink_t = appfwk::DAQSink<std::unique_ptr<dataformats::TriggerRecord>>;

  bool read_queues( trigger_decision_source_t & , 
		    fragment_sources_t &, 
		    bool drain = false ) ;
  
  dataformats::TriggerRecord* build_trigger_record(const TriggerId&);
  // build_trigger_record will allocate memory and then orphan it to the caller via the returned pointer
  // Plese note that the method will destroy the memory saved in the bookkeeping map

  bool send_trigger_record( const TriggerId&, trigger_record_sink_t &, 
			    std::atomic<bool> & running ) ;
  // this creates a trigger record and send it
  
private:
  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);

  // Threading
  dunedaq::appfwk::ThreadHelper m_thread;
  void do_work(std::atomic<bool>&);

  // Configuration
  // size_t m_sleep_msec_while_running;
  std::chrono::milliseconds m_queue_timeout;

  // Input Queues
  std::string m_trigger_decision_source_name;
  std::vector<std::string> m_fragment_source_names;

  // Output queues
  std::string m_trigger_record_sink_name;

  // bookeeping
  std::map<TriggerId, std::vector<std::unique_ptr<dataformats::Fragment>>> m_fragments;
  std::map<TriggerId, dfmessages::TriggerDecision> m_trigger_decisions;

  dataformats::timestamp_diff_t m_max_time_difference;
  dataformats::timestamp_t m_current_time = 0;

};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_FRAGMENTRECEIVER_HPP_
