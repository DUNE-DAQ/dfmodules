/**
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "HDF5DataStore.hpp"

namespace dunedaq {
namespace dfmodules {

HDF5DataStore::HDF5DataStore(const nlohmann::json& conf)
  : DataStore(conf.value("name", "data_store"))
  , m_basic_name_of_open_file("")
  , m_open_flags_of_open_file(0)
  , m_username_substring_for_filename("_UnknownUser")
  , m_timestamp_substring_for_filename("_UnknownTime")
  , m_hashed_timeuser_substring_for_filename("_abcdef")
{
  TLOG(TLVL_DEBUG) << get_name() << ": Configuration: " << conf;

  m_config_params = conf.get<hdf5datastore::ConfParams>();
  m_operation_mode = m_config_params.mode;
  m_path = m_config_params.directory_path;
  m_max_file_size = m_config_params.max_file_size_bytes;
  m_disable_unique_suffix = m_config_params.disable_unique_filename_suffix;

  m_file_count = 0;
  m_recorded_size = 0;

  if (m_operation_mode != "one-event-per-file" && m_operation_mode != "one-fragment-per-file" &&
      m_operation_mode != "all-per-file") {

    throw InvalidOperationMode(ERS_HERE, get_name(), m_operation_mode);
  }

  char* unp = getenv("USER");
  std::string tmp_string(unp);
  m_username_substring_for_filename = "_" + tmp_string;
  TLOG(TLVL_DEBUG) << get_name() << ": m_username_substring_for_filename: " << m_username_substring_for_filename;
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DATA_STORE(dunedaq::dfmodules::HDF5DataStore)
