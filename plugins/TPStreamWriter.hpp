/**
 * @file TPStreamWriter.hpp
 *
 * TPStreamWriter is a DAQModule that provides sample code for writing TPSets to disk.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_TPSTREAMWRITER_HPP_
#define DFMODULES_PLUGINS_TPSTREAMWRITER_HPP_

#include "dfmodules/DataStore.hpp"

#include "appfwk/DAQModule.hpp"
#include "iomanager/Receiver.hpp"
#include "daqdataformats/TimeSlice.hpp"
#include "trigger/TPSet.hpp"
#include "utilities/WorkerThread.hpp"

#include <memory>
#include <string>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief TPStreamWriter receives TPSets from a queue and prints them out
 */
class TPStreamWriter : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief TPStreamWriter Constructor
   * @param name Instance name for this TPStreamWriter instance
   */
  explicit TPStreamWriter(const std::string& name);

  TPStreamWriter(const TPStreamWriter&) = delete;            ///< TPStreamWriter is not copy-constructible
  TPStreamWriter& operator=(const TPStreamWriter&) = delete; ///< TPStreamWriter is not copy-assignable
  TPStreamWriter(TPStreamWriter&&) = delete;                 ///< TPStreamWriter is not move-constructible
  TPStreamWriter& operator=(TPStreamWriter&&) = delete;      ///< TPStreamWriter is not move-assignable

  void init(const data_t&) override;
  void get_info(opmonlib::InfoCollector& ci, int level) override;

private:
  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);
  void do_scrap(const data_t&);

  // Threading
  dunedaq::utilities::WorkerThread m_thread;
  void do_work(std::atomic<bool>&);

  // Configuration
  std::chrono::milliseconds m_queue_timeout;
  size_t m_accumulation_interval_ticks;
  daqdataformats::run_number_t m_run_number;

  // Queue sources and sinks
  using incoming_t = trigger::TPSet;
  using source_t = iomanager::ReceiverConcept<incoming_t>;
  std::shared_ptr<source_t> m_tpset_source;

  // Worker(s)
  std::unique_ptr<DataStore> m_data_writer;

  // Metrics
  std::atomic<uint64_t> m_tpset_received = { 0 };         // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_tpset_written  = { 0 };         // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_bytes_output   = { 0 };         // NOLINT(build/unsigned)

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
                       "A problem was encountered when writing TimeSlice number " << trnum << " in run " << runnum,
                       ((std::string)name),
                       ((size_t)trnum)((size_t)runnum))

} // namespace dunedaq

#endif // DFMODULES_PLUGINS_TPSTREAMWRITER_HPP_
