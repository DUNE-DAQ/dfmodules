/**
 * @file DataWriter.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_DATAWRITER_HPP_
#define DFMODULES_PLUGINS_DATAWRITER_HPP_

#include "dfmodules/DataStore.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSink.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"
#include "daqdataformats/TriggerRecord.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief DataWriter is a shell for what we might write for the MiniDAQApp.
 */
class DataWriter : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief DataWriter Constructor
   * @param name Instance name for this DataWriter instance
   */
  explicit DataWriter(const std::string& name);

  DataWriter(const DataWriter&) = delete;            ///< DataWriter is not copy-constructible
  DataWriter& operator=(const DataWriter&) = delete; ///< DataWriter is not copy-assignable
  DataWriter(DataWriter&&) = delete;                 ///< DataWriter is not move-constructible
  DataWriter& operator=(DataWriter&&) = delete;      ///< DataWriter is not move-assignable

  void init(const data_t&) override;
  void get_info(opmonlib::InfoCollector& ci, int level) override;

private:
  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);
  void do_scrap(const data_t&);

  // Threading
  dunedaq::appfwk::ThreadHelper m_thread;
  void do_work(std::atomic<bool>&);

  // Configuration
  // size_t m_sleep_msec_while_running;
  std::chrono::milliseconds m_queue_timeout;
  bool m_data_storage_is_enabled;
  int m_data_storage_prescale;
  daqdataformats::run_number_t m_run_number;
  size_t m_min_write_retry_time_usec;
  size_t m_max_write_retry_time_usec;
  int m_write_retry_time_increase_factor;

  // Queue(s)
  using trigrecsource_t = dunedaq::appfwk::DAQSource<std::unique_ptr<daqdataformats::TriggerRecord>>;
  std::unique_ptr<trigrecsource_t> m_trigger_record_input_queue;

  // Connection(s)
  std::string m_trigger_decision_token_connection;
  std::string m_trigger_decision_connection;

  // Worker(s)
  std::unique_ptr<DataStore> m_data_writer;

  // Metrics
  std::atomic<uint64_t> m_records_received = { 0 };     // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_records_received_tot = { 0 }; // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_records_written = { 0 };      // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_records_written_tot = { 0 };  // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_bytes_output = { 0 };         // NOLINT(build/unsigned)

  // Other
  std::map<daqdataformats::trigger_number_t, size_t> m_seqno_counts;

  inline double elapsed_seconds(std::chrono::steady_clock::time_point then,
                                std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const
  {
    return std::chrono::duration_cast<std::chrono::seconds>(now - then).count();
  }

  using geoid_system_type_t = daqdataformats::GeoID::SystemType;
  using key_group_type_t = StorageKey::DataRecordGroupType;
  std::map<geoid_system_type_t, key_group_type_t> m_system_type_to_group_type_mapping;
  key_group_type_t get_group_type(geoid_system_type_t system_type, daqdataformats::FragmentType frag_type)
  {
    // 16-Aug-2021, KAB: ugly hack
    if (system_type == geoid_system_type_t::kTPC && frag_type == daqdataformats::FragmentType::kTriggerPrimitives) {
      return key_group_type_t::kTPC_TP;
    }

    if (m_system_type_to_group_type_mapping.size() == 0) {
      m_system_type_to_group_type_mapping[geoid_system_type_t::kTPC] = key_group_type_t::kTPC;
      m_system_type_to_group_type_mapping[geoid_system_type_t::kPDS] = key_group_type_t::kPDS;
      m_system_type_to_group_type_mapping[geoid_system_type_t::kDataSelection] = key_group_type_t::kTrigger;
      m_system_type_to_group_type_mapping[geoid_system_type_t::kNDLArTPC] = key_group_type_t::kNDLArTPC;
      m_system_type_to_group_type_mapping[geoid_system_type_t::kInvalid] = key_group_type_t::kInvalid;
    }
    auto map_iter = m_system_type_to_group_type_mapping.find(system_type);
    if (map_iter == m_system_type_to_group_type_mapping.end()) {
      return key_group_type_t::kInvalid;
    }
    return map_iter->second;
  };
};
} // namespace dfmodules

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidDataWriter,
                       appfwk::GeneralDAQModuleIssue,
                       "A valid dataWriter instance is not available so it will not be possible to write data. A "
                       "likely cause for this is a skipped or missed Configure transition.",
                       ((std::string)name),
                       ERS_EMPTY)

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       DataWritingProblem,
                       appfwk::GeneralDAQModuleIssue,
                       "A problem was encountered when writing TriggerRecord number " << trnum << " in run " << runnum,
                       ((std::string)name),
                       ((size_t)trnum)((size_t)runnum))

} // namespace dunedaq

#endif // DFMODULES_PLUGINS_DATAWRITER_HPP_
