/**
 * @file TPSetWriter.hpp
 *
 * TPSetWriter is a DAQModule that provides sample code for writing TPSets to disk.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_TPSETWRITER_HPP_
#define DFMODULES_PLUGINS_TPSETWRITER_HPP_

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"
#include "trigger/TPSet.hpp"

#include <memory>
#include <string>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief TPSetWriter receives TPSets from a queue and prints them out
 */
class TPSetWriter : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief TPSetWriter Constructor
   * @param name Instance name for this TPSetWriter instance
   */
  explicit TPSetWriter(const std::string& name);

  TPSetWriter(const TPSetWriter&) = delete;            ///< TPSetWriter is not copy-constructible
  TPSetWriter& operator=(const TPSetWriter&) = delete; ///< TPSetWriter is not copy-assignable
  TPSetWriter(TPSetWriter&&) = delete;                 ///< TPSetWriter is not move-constructible
  TPSetWriter& operator=(TPSetWriter&&) = delete;      ///< TPSetWriter is not move-assignable

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
  std::chrono::milliseconds m_queue_timeout;
  size_t m_max_file_size;
  daqdataformats::run_number_t m_run_number;

  // Queue sources and sinks
  using source_t = appfwk::DAQSource<trigger::TPSet>;
  std::unique_ptr<source_t> m_tpset_source;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_TPSETWRITER_HPP_
