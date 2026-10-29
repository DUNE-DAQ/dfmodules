/**
 *
 * @file HDF5DataStore.hpp
 *
 * Header for an implementation of the DataStore interface which writes out *.hdf5 files
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_HDF5DATASTORE_HPP_
#define DFMODULES_PLUGINS_HDF5DATASTORE_HPP_

#include "dfmodules/FileDataStoreImpl.hpp"
#include "dfmodules/RawDataFileConfig.hpp"

#include "appmodel/DataStoreConf.hpp"
#include "hdf5libs/HDF5RawDataFile.hpp"
#include "rawdatafilelibs/FileMetadata.hpp"

#include <memory>
#include <string>

namespace dunedaq::dfmodules {

class HDF5DataStore : public FileDataStoreImpl<hdf5libs::HDF5RawDataFile>
{

public:
  explicit HDF5DataStore(std::string const& name,
                         std::shared_ptr<appfwk::ConfigurationManager> mcfg,
                         std::string const& writer_name)
    : FileDataStoreImpl(name, mcfg, writer_name)
    , m_source_id_geo_id_map{ make_source_id_geo_id_map(get_session()) }
    , m_file_layout_parameters{ make_hdf5_file_layout_parameters(*get_configuration().get_file_layout_params()) }
  {
  }

  void open_new_file(const std::string& unique_filename) override;

private:
  // Built once from the configuration.
  const daqdataformats::SourceIDGeoIDMap m_source_id_geo_id_map;
  const hdf5libs::HDF5FileLayoutParameters m_file_layout_parameters;
};

inline void
HDF5DataStore::open_new_file(const std::string& unique_filename)
{
  rawdatafilelibs::FileMetadata metadata;
  metadata.run_number = get_run_number();
  metadata.file_index = get_file_index();
  metadata.application_name = get_application_name();
  metadata.record_type = rawdatafilelibs::string_to_record_type(m_file_layout_parameters.record_name_prefix);
  metadata.creation_timestamp_ms = get_file_creation_timestamp_ms(); // the same time as in the file name
  metadata.operational_environment = get_operational_environment();
  metadata.offline_data_stream = get_offline_data_stream();
  metadata.run_is_for_test_purposes = get_run_is_for_test_purposes();
  metadata.compression_level = get_compression_level();

  // The file is always created exclusively; any exception is wrapped in a FileOperationProblem by
  // FileDataStoreImpl.
  auto& file_handle{ get_file_handle() };
  file_handle = std::make_unique<hdf5libs::HDF5RawDataFile>(
    unique_filename, metadata, m_file_layout_parameters, m_source_id_geo_id_map, ".writing");

  TLOG_DEBUG(TLVL_BASIC) << get_name() << "Created HDF5 file (" << unique_filename << ").";
}

} // namespace dunedaq::dfmodules

#endif // DFMODULES_PLUGINS_HDF5DATASTORE_HPP_
