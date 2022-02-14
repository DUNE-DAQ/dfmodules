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

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSource.hpp"
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
  size_t m_max_file_size;
  daqdataformats::run_number_t m_run_number;

  // Queue sources and sinks
  using source_t = appfwk::DAQSource<trigger::TPSet>;
  std::unique_ptr<source_t> m_tpset_source;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_TPSTREAMWRITER_HPP_
