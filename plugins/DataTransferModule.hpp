/**
 * @file DataTransferModule.hpp
 *
 * DataTransferModule is a simple module that transfers data from
 * one DataStore to another.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_DATATRANSFERMODULE_HPP_
#define DFMODULES_PLUGINS_DATATRANSFERMODULE_HPP_

#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/DataStore.hpp"

#include <appfwk/DAQModule.hpp>
#include <appfwk/ThreadHelper.hpp>
#include <ers/Issue.h>

#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief DataTransferModule transfers data from one DataStore to another.
 */
class DataTransferModule : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief DataTransferModule Constructor
   * @param name Instance name for this DataTransferModule instance
   */
  explicit DataTransferModule(const std::string& name);

  DataTransferModule(const DataTransferModule&) = delete;            ///< DataTransferModule is not copy-constructible
  DataTransferModule& operator=(const DataTransferModule&) = delete; ///< DataTransferModule is not copy-assignable
  DataTransferModule(DataTransferModule&&) = delete;                 ///< DataTransferModule is not move-constructible
  DataTransferModule& operator=(DataTransferModule&&) = delete;      ///< DataTransferModule is not move-assignable

  void init(const data_t&) override;

private:
  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);
  void do_unconfigure(const data_t&);

  // Threading
  dunedaq::appfwk::ThreadHelper thread_;
  void do_work(std::atomic<bool>&);

  // Configuration defaults
  const size_t REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING = 1000;

  // Configuration
  size_t m_sleep_msec_wile_running = REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING;

  // Workers
  std::unique_ptr<DataStore> m_input_data_store;
  std::unique_ptr<DataStore> m_output_data_store;
};
} // namespace dfmodules

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidDataStoreError,
                       appfwk::GeneralDAQModuleIssue,
                       "A valid dataStore instance is not available for "
                         << operation
                         << ", so it will not be possible to combine data. A likely cause for this is a skipped or "
                            "missed Configure transition.",
                       ((std::string)name),
                       ((std::string)operation))

} // namespace dunedaq

#endif // DFMODULES_PLUGINS_DATATRANSFERMODULE_HPP_
