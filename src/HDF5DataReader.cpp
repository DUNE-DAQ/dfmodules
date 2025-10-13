/**
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/HDF5DataReader.hpp"

/**
 * @brief HDF5DataReader Constructor
 * @param name, path, filename, operationMode
 *
 */
dunedaq::dfmodules::HDF5DataReader::HDF5DataReader(std::string const& hostname)
  : m_file_handles()
  , m_host_name(hostname)
{
}

dunedaq::dfmodules::HDF5DataReader::~HDF5DataReader()
{
  std::lock_guard<std::mutex> lk(m_file_handles_mutex);
  m_file_handles.clear();
}

std::optional<dunedaq::daqdataformats::TriggerRecord>
dunedaq::dfmodules::HDF5DataReader::read_trigger_record(std::string const& file,
                                                        dunedaq::daqdataformats::trigger_number_t trigger,
                                                        dunedaq::daqdataformats::sequence_number_t sequence)
{
  std::lock_guard<std::mutex> lk(m_file_handles_mutex);

  if (!m_file_handles.count(file) || m_file_handles[file] == nullptr) {
    m_file_handles[file] = std::make_shared<hdf5libs::HDF5RawDataFile>(file);
  }

  try {
    return m_file_handles[file]->get_trigger_record(trigger, sequence);
  } catch (hdf5libs::RecordIDNotFound&) {
    m_file_handles.erase(file);
  }

  return std::nullopt;
}

std::optional<dunedaq::daqdataformats::TimeSlice>
dunedaq::dfmodules::HDF5DataReader::read_time_slice(std::string const& file,
                                                    dunedaq::daqdataformats::timeslice_number_t number)
{
  std::lock_guard<std::mutex> lk(m_file_handles_mutex);

  if (!m_file_handles.count(file) || m_file_handles[file] == nullptr) {
    m_file_handles[file] = std::make_shared<hdf5libs::HDF5RawDataFile>(file);
  }
  try {
    return m_file_handles[file]->get_timeslice(number);
  } catch (hdf5libs::RecordIDNotFound&) {
    m_file_handles.erase(file);
  }

  return std::nullopt;
}

void
dunedaq::dfmodules::HDF5DataReader::done_with_file(std::string const& file)
{
  std::lock_guard<std::mutex> lk(m_file_handles_mutex);
  m_file_handles.erase(file);
}
