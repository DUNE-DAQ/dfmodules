/**
 * @file DataGenerator.hpp
 *
 * DataGenerator is a simple DAQModule implementation that
 * writes data to HDF5 file(s) on disk.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_DATAGENERATOR_HPP_
#define DFMODULES_PLUGINS_DATAGENERATOR_HPP_

#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/DataStore.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/ThreadHelper.hpp"
#include "dataformats/Types.hpp"
#include "ers/Issue.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief DataGenerator creates fake events and writes
 * them to one or more HDF5 files..
 */
class DataGenerator : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief DataGenerator Constructor
   * @param name Instance name for this DataGenerator instance
   */
  explicit DataGenerator(const std::string& name);

  DataGenerator(const DataGenerator&) = delete;            ///< DataGenerator is not copy-constructible
  DataGenerator& operator=(const DataGenerator&) = delete; ///< DataGenerator is not copy-assignable
  DataGenerator(DataGenerator&&) = delete;                 ///< DataGenerator is not move-constructible
  DataGenerator& operator=(DataGenerator&&) = delete;      ///< DataGenerator is not move-assignable

  void init(const data_t&) override;

private:
  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);
  void do_unconfigure(const data_t&);

  // Threading
  dunedaq::appfwk::ThreadHelper m_thread;
  void do_work(std::atomic<bool>&);

  // Configuration defaults
  const size_t m_reasonable_default_geo_loc_count = 5;
  const size_t m_reasonable_default_sleep_msec = 1000;
  const size_t m_reasonable_default_io_size_bytes = 1024;

  // Configuration
  size_t m_geo_loc_count = m_reasonable_default_geo_loc_count;
  size_t m_io_size = m_reasonable_default_io_size_bytes;
  size_t m_sleep_msec_while_running = m_reasonable_default_sleep_msec;
  dunedaq::dataformats::run_number_t m_run_number;

  // Workers
  std::unique_ptr<DataStore> m_data_writer;
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

#endif // DFMODULES_PLUGINS_DATAGENERATOR_HPP_
