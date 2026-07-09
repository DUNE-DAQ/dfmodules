/**
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_HDF5DATASTORE_HPP_
#define DFMODULES_PLUGINS_HDF5DATASTORE_HPP_

#include "dfmodules/FileDataStoreImpl.hpp"

#include "appmodel/DataStoreConf.hpp"
#include "hdf5libs/HDF5RawDataFile.hpp"

#include <string>

namespace dunedaq::dfmodules {

  class HDF5DataStore : public FileDataStoreImpl<hdf5libs::HDF5RawDataFile,
					     appmodel::DataStoreConf> {

  public:
    explicit HDF5DataStore(std::string const& name,
                         std::shared_ptr<appfwk::ConfigurationManager> mcfg,
			   std::string const& writer_name) :
      FileDataStoreImpl(name, mcfg, writer_name)
    {}

    void open_new_file(const std::string& unique_filename) override;
  };

  inline void HDF5DataStore::open_new_file(const std::string& unique_filename) {

    auto& file_handle {get_file_handle()};
    try {
      file_handle.reset(
			      new hdf5libs::HDF5RawDataFile(unique_filename,
                                        get_run_number(),
                                        get_file_index(),
							    get_application_name(),
							    get_configuration().get_file_layout_params(),
							    hdf5libs::HDF5SourceIDHandler::make_source_id_geo_id_map(get_session()),
							    get_compression_level(),
                                        ".writing",
							    HighFive::File::OpenOrCreate));
    } catch (std::exception const& excpt) {
        throw FileOperationProblem(ERS_HERE, get_name(), unique_filename, excpt);
    } catch (...) { // NOLINT(runtime/exceptions)                                                                                             
        // NOLINT here because we *ARE* re-throwing the exception!                                                                              
        throw FileOperationProblem(ERS_HERE, get_name(), unique_filename);
    }

    TLOG_DEBUG(TLVL_BASIC) << get_name() << "Created HDF5 file (" << unique_filename << ").";

    // write attributes that aren't being handled by the HDF5RawDataFile right now
    file_handle->write_attribute("operational_environment", (std::string)get_operational_environment());
    file_handle->write_attribute("offline_data_stream", (std::string)get_offline_data_stream());
    file_handle->write_attribute("run_was_for_test_purposes", (std::string)(get_run_is_for_test_purposes() ? "true" : "false"));
  }
  
} // namespace dunedaq::dfmodules

#endif // DFMODULES_PLUGINS_HDF5DATASTORE_HPP_

