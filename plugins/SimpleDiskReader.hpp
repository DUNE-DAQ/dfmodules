/**
 * @file SimpleDiskReader.hpp
 *
 * SimpleDiskReader is a simple DAQModule implementation that
 * periodically writes data to an HDF5 file on disk.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DDPDEMO_SRC_SIMPLEDISKREADER_HPP_
#define DDPDEMO_SRC_SIMPLEDISKREADER_HPP_

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
 * @brief SimpleDiskReader reads fake events and dumps them on the screen
 * them to an HDF5 file..
 */
class SimpleDiskReader : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief SimpleDiskReader Constructor
   * @param name Instance name for this SimpleDiskReader instance
   */
  explicit SimpleDiskReader(const std::string& name);

  SimpleDiskReader(const SimpleDiskReader&) = delete;            ///< SimpleDiskReader is not copy-constructible
  SimpleDiskReader& operator=(const SimpleDiskReader&) = delete; ///< SimpleDiskReader is not copy-assignable
  SimpleDiskReader(SimpleDiskReader&&) = delete;                 ///< SimpleDiskReader is not move-constructible
  SimpleDiskReader& operator=(SimpleDiskReader&&) = delete;      ///< SimpleDiskReader is not move-assignable

  void init( const data_t& ) override;

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
  const size_t REASONABLE_DEFAULT_INTFRAGMENT = 1;
  const size_t REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING = 1000;

  const std::string DEFAULT_PATH = ".";
  const std::string DEFAULT_FILENAME = "demo.hdf5";

  // Configuration
  size_t key_eventID_ = REASONABLE_DEFAULT_INTFRAGMENT;
  std::string key_detectorID_ = "FELIX";
  size_t key_geoLocationID_ = REASONABLE_DEFAULT_INTFRAGMENT;
  size_t sleepMsecWhileRunning_ = REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING;

  std::string filename_pattern_ = DEFAULT_FILENAME;

  // Workers
  std::unique_ptr<DataStore> dataReader_;
};
} // namespace ddpdemo

ERS_DECLARE_ISSUE_BASE(ddpdemo,
                       ProgressUpdate,
                       appfwk::GeneralDAQModuleIssue,
                       message,
                       ((std::string)name),
                       ((std::string)message))

ERS_DECLARE_ISSUE_BASE(ddpdemo,
                       InvalidDataReaderError,
                       appfwk::GeneralDAQModuleIssue,
                       "A valid dataReader instance is not available so it will not be possible to read data. A likely "
                       "cause for this is a skipped or missed Configure transition.",
                       ((std::string)name),
                       ERS_EMPTY)

} // namespace dunedaq

#endif // DDPDEMO_SRC_SIMPLEDISKREADER_HPP_
