/**
 * @file HDF5DataReader.hpp
 *
 * Reads files written by HDF5DataStore and extracts TriggerRecords or TimeSlices
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DFMODULES_HDF5DATAREADER_HPP_
#define DFMODULES_SRC_DFMODULES_HDF5DATAREADER_HPP_

#include "hdf5libs/HDF5RawDataFile.hpp"

#include "appfwk/DAQModule.hpp"
#include "logging/Logging.hpp" // NOTE: if ISSUES ARE DECLARED BEFORE include logging/Logging.hpp, TLOG_DEBUG<<issue wont work.

#include <cstdlib>
#include <memory>
#include <string>

namespace dunedaq {

namespace dfmodules {

/**
 * @brief HDF5DataReader reads TriggerRecords and TimeSlices from HDF5 files
 */
class HDF5DataReader
{

public:
  enum
  {
    TLVL_BASIC = 2,
    TLVL_FILE_SIZE = 5
  };

  /**
   * @brief HDF5DataReader Constructor
   * @param name, path, filename, operationMode
   *
   */
  explicit HDF5DataReader(std::string const& hostname);

  virtual ~HDF5DataReader();

  std::optional<daqdataformats::TriggerRecord> read_trigger_record(std::string const& file,
                                                                   daqdataformats::trigger_number_t trigger,
                                                                   daqdataformats::sequence_number_t sequence);
  std::optional<daqdataformats::TimeSlice> read_time_slice(std::string const& file,
                                                           daqdataformats::timeslice_number_t number);

  void done_with_file(std::string const& file);

protected:
private:
  HDF5DataReader(const HDF5DataReader&) = delete;
  HDF5DataReader& operator=(const HDF5DataReader&) = delete;
  HDF5DataReader(HDF5DataReader&&) = delete;
  HDF5DataReader& operator=(HDF5DataReader&&) = delete;

  std::mutex m_file_handles_mutex;
  std::unordered_map<std::string, std::shared_ptr<hdf5libs::HDF5RawDataFile>> m_file_handles;
  std::string m_host_name;
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_DFMODULES_HDF5DATAREADER_HPP_

// Local Variables:
// c-basic-offset: 2
// End:
