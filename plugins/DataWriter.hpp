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
#include "dataformats/TriggerRecord.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"

#include <chrono>
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
  int m_initial_tokens;
  dataformats::run_number_t m_run_number;

  // Queue(s)
  using trigrecsource_t = dunedaq::appfwk::DAQSource<std::unique_ptr<dataformats::TriggerRecord>>;
  std::unique_ptr<trigrecsource_t> m_trigger_record_input_queue;
  using tokensink_t = dunedaq::appfwk::DAQSink<dfmessages::TriggerDecisionToken>;
  std::unique_ptr<tokensink_t> m_trigger_decision_token_output_queue;

  // Worker(s)
  std::unique_ptr<DataStore> m_data_writer;

  inline double elapsed_seconds(std::chrono::steady_clock::time_point then,
                                std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const
  {
    return std::chrono::duration_cast<std::chrono::seconds>(now - then).count();
  }
};
} // namespace dfmodules

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidDataWriterError,
                       appfwk::GeneralDAQModuleIssue,
                       "A valid dataWriter instance is not available so it will not be possible to write data. A "
                       "likely cause for this is a skipped or missed Configure transition.",
                       ((std::string)name),
                       ERS_EMPTY)

} // namespace dunedaq

#endif // DFMODULES_PLUGINS_DATAWRITER_HPP_
