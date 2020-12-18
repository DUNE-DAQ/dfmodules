/**
 * @file GetAllKeysTest.hpp
 *
 * GetAllKeysTest is a simple DAQModule implementation that
 * periodically writes data to an HDF5 file on disk.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DDPDEMO_SRC_GETALLKEYSTEST_HPP_
#define DDPDEMO_SRC_GETALLKEYSTEST_HPP_

#include "ddpdemo/DataStore.hpp"

#include <appfwk/DAQModule.hpp>
#include <appfwk/ThreadHelper.hpp>
#include <ers/Issue.h>

#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace ddpdemo {

/**
 * @brief GetAllKeysTest creates fake events writes
 * them to an HDF5 file..
 */
class GetAllKeysTest : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief GetAllKeysTest Constructor
   * @param name Instance name for this GetAllKeysTest instance
   */
  explicit GetAllKeysTest(const std::string& name);

  GetAllKeysTest(const GetAllKeysTest&) = delete;            ///< GetAllKeysTest is not copy-constructible
  GetAllKeysTest& operator=(const GetAllKeysTest&) = delete; ///< GetAllKeysTest is not copy-assignable
  GetAllKeysTest(GetAllKeysTest&&) = delete;                 ///< GetAllKeysTest is not move-constructible
  GetAllKeysTest& operator=(GetAllKeysTest&&) = delete;      ///< GetAllKeysTest is not move-assignable

  void init(  const data_t&  ) override;

private:
  // Commands
  void do_configure(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);
  void do_unconfigure(const data_t&);

  // Threading
  dunedaq::appfwk::ThreadHelper thread_;
  void do_work(std::atomic<bool>&);

  // Configuration defaults
  const size_t REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING = 1000;

  // Configuration
  size_t sleepMsecWhileRunning_ = REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING;

  // Workers
  std::unique_ptr<DataStore> dataStore_;
};
} // namespace ddpdemo

ERS_DECLARE_ISSUE_BASE(ddpdemo,
                       ProgressUpdate,
                       appfwk::GeneralDAQModuleIssue,
                       message,
                       ((std::string)name),
                       ((std::string)message))

} // namespace dunedaq

#endif // DDPDEMO_SRC_GETALLKEYSTEST_HPP_
