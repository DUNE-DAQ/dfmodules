/**
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_HDF5DATASTORE_HPP_
#define DFMODULES_PLUGINS_HDF5DATASTORE_HPP_

#include "dfmodules/DataStoreImpl.hpp"

#include "hdf5libs/HDF5RawDataFile.hpp"

#include <string>

namespace dunedaq::dfmodules {

  class HDF5DataStore : public DataStoreImpl<hdf5libs::HDF5RawDataFile,
					     HighFive::File::OpenOrCreate,
					     hdf5libs::TimeSliceAlreadyExists> {

  public:
    explicit HDF5DataStore(std::string const& name,
                         std::shared_ptr<appfwk::ConfigurationManager> mcfg,
			   std::string const& writer_name) :
      DataStoreImpl(name, mcfg, writer_name)
    {}

    void open_new_file(const std::string& unique_filename) override;
  };

  inline void HDF5DataStore::open_new_file(const std::string& unique_filename) {

    try {
      m_file_handle.reset(
			  new hdf5libs::HDF5RawDataFile(unique_filename,
                                        m_run_number,
                                        m_file_index,
                                        m_writer_identifier,
                                        m_config_params->get_file_layout_params(),
                                        hdf5libs::HDF5SourceIDHandler::make_source_id_geo_id_map(m_session),
                                        m_compression_level,
                                        ".writing",
                                        m_open_flags_of_open_file));
    } catch (std::exception const& excpt) {
        throw FileOperationProblem(ERS_HERE, get_name(), unique_filename, excpt);
    } catch (...) { // NOLINT(runtime/exceptions)                                                                                             
        // NOLINT here because we *ARE* re-throwing the exception!                                                                              
        throw FileOperationProblem(ERS_HERE, get_name(), unique_filename);
    }

    if (m_open_flags_of_open_file == HighFive::File::ReadOnly) {
      TLOG_DEBUG(TLVL_BASIC) << get_name() << "Opened HDF5 file read-only.";
    } else {
      TLOG_DEBUG(TLVL_BASIC) << get_name() << "Created HDF5 file (" << unique_filename << ").";

      // write attributes that aren't being handled by the HDF5RawDataFile right now                                                          
      // m_file_handle->write_attribute("data_format_version",(int)m_key_translator_ptr->get_current_version());                              
      m_file_handle->write_attribute("operational_environment", (std::string)m_operational_environment);
      m_file_handle->write_attribute("offline_data_stream", (std::string)m_offline_data_stream);
      m_file_handle->write_attribute("run_was_for_test_purposes", (std::string)(m_run_is_for_test_purposes ? "true" : "false"));
    }
  }
  
} // namespace dunedaq::dfmodules

#endif // DFMODULES_PLUGINS_HDF5DATASTORE_HPP_

