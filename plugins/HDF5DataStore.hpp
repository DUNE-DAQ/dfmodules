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

#include "dfmodules/FileDataStore.hpp"
#include "dfmodules/HDF5FileConfig.hpp"

#include "appmodel/DataStoreConf.hpp"
#include "appmodel/HDF5DataStoreConf.hpp"
#include "hdf5libs/HDF5Backend.hpp"
#include "rawdatafilelibs/Backend.hpp"
#include "rawdatafilelibs/FileMetadata.hpp"

#include <memory>
#include <optional>
#include <string>

namespace dunedaq::dfmodules {

/**
 * @brief Writes DUNE DAQ HDF5 files, with hdf5libs' HDF5 backend. Configured with an
 * appmodel::HDF5DataStoreConf (or, during the transition, a plain DataStoreConf).
 */
class HDF5DataStore : public FileDataStore
{

public:
  explicit HDF5DataStore(std::string const& name,
                         std::shared_ptr<appfwk::ConfigurationManager> mcfg,
                         std::string const& writer_name)
    : FileDataStore(name, mcfg, writer_name)
    , m_file_layout_parameters{ find_file_layout_parameters() }
  {
  }

protected:
  std::string get_file_name_extension() const override { return "hdf5"; }

  std::unique_ptr<rawdatafilelibs::WriterBackend> make_writer_backend(
    rawdatafilelibs::RecordType /*record_type*/) override
  {
    hdf5libs::HDF5WriterOptions options;
    options.file_layout_parameters = m_file_layout_parameters; // unset: the standard layout for the record type
    return std::make_unique<hdf5libs::HDF5WriterBackend>(options);
  }

private:
  // The HDF5 layout parameters: from HDF5DataStoreConf.hdf5_file_layout_params, else from the deprecated
  // DataStoreConf.file_layout_params, else none (the standard layout)
  std::optional<hdf5libs::HDF5FileLayoutParameters> find_file_layout_parameters() const
  {
    auto const* hdf5_conf = get_configuration().cast<appmodel::HDF5DataStoreConf>();
    if (hdf5_conf != nullptr && hdf5_conf->get_hdf5_file_layout_params() != nullptr) {
      return make_hdf5_file_layout_parameters(*hdf5_conf->get_hdf5_file_layout_params());
    }
    if (get_configuration().get_file_layout_params() != nullptr) {
      ers::warning(DeprecatedDataStoreConfiguration(
        ERS_HERE,
        get_name(),
        "DataStoreConf.file_layout_params is deprecated; use an HDF5DataStoreConf with hdf5_file_layout_params"));
      return make_hdf5_file_layout_parameters(*get_configuration().get_file_layout_params());
    }
    return std::nullopt;
  }

  const std::optional<hdf5libs::HDF5FileLayoutParameters> m_file_layout_parameters;
};

} // namespace dunedaq::dfmodules

#endif // DFMODULES_PLUGINS_HDF5DATASTORE_HPP_
