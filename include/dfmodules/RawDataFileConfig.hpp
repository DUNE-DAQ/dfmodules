/**
 * @file RawDataFileConfig.hpp Translating configuration objects into what raw data files store
 *
 * Raw data files are interpreted on their own, so the parts of the configuration they need
 * (which GeoIDs feed each SourceID, how HDF5 groups are named) are copied into them. The
 * translation lives here, with the DataStores, so that the file libraries don't depend on the
 * configuration packages.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_INCLUDE_DFMODULES_RAWDATAFILECONFIG_HPP_
#define DFMODULES_INCLUDE_DFMODULES_RAWDATAFILECONFIG_HPP_

#include "hdf5libs/HDF5FileLayoutParameters.hpp"

#include "appmodel/HDF5FileLayoutParams.hpp"
#include "appmodel/HDF5PathParams.hpp"
#include "appmodel/ReadoutApplication.hpp"
#include "confmodel/Application.hpp"
#include "confmodel/DetectorStream.hpp"
#include "confmodel/DetectorToDaqConnection.hpp"
#include "confmodel/GeoId.hpp"
#include "confmodel/Session.hpp"
#include "daqdataformats/SourceID.hpp"
#include "daqdataformats/SourceIDGeoIDMap.hpp"
#include "detdataformats/DetID.hpp"
#include "detdataformats/GeoID.hpp"
#include "logging/Logging.hpp"

#include <cstdint>

namespace dunedaq::dfmodules {

/**
 * @brief The GeoIDs of the detector streams of every enabled readout connection in the session,
 * by the SourceID that each stream's data is sent with.
 */
inline daqdataformats::SourceIDGeoIDMap
make_source_id_geo_id_map(const confmodel::Session& session)
{
  daqdataformats::SourceIDGeoIDMap geo_id_map;

  for (auto const& app : session.all_applications()) {
    auto ro_app = app->cast<appmodel::ReadoutApplication>();
    if (!ro_app) {
      continue;
    }

    for (auto d2d_conn : ro_app->get_detector_connections()) {
      if (d2d_conn->is_excluded(session)) {
        TLOG_DEBUG(7) << "Ignoring excluded DetectorToDaqConnection " << d2d_conn->UID();
        continue;
      }
      TLOG_DEBUG(7) << "Processing DetectorToDaqConnection " << d2d_conn->UID();

      for (auto resource : d2d_conn->streams()) {
        if (resource->is_excluded(session)) {
          TLOG_DEBUG(7) << "Ignoring excluded DetectorStream " << resource->UID();
          continue;
        }
        auto stream = resource->cast<confmodel::DetectorStream>();
        if (!stream) {
          continue;
        }
        auto geo_id = stream->get_geo_id();
        geo_id_map.add(
          daqdataformats::SourceID(daqdataformats::SourceID::Subsystem::kDetectorReadout, stream->get_source_id()),
          detdataformats::GeoID(static_cast<detdataformats::DetID::Subdetector>(geo_id->get_detector_id()),
                                static_cast<uint16_t>(geo_id->get_crate_id()),    // NOLINT(build/unsigned)
                                static_cast<uint16_t>(geo_id->get_slot_id()),     // NOLINT(build/unsigned)
                                static_cast<uint16_t>(geo_id->get_stream_id()))); // NOLINT(build/unsigned)
      }
    }
  }
  return geo_id_map;
}

/**
 * @brief The HDF5 group and dataset naming parameters, from their configuration object
 */
inline hdf5libs::HDF5FileLayoutParameters
make_hdf5_file_layout_parameters(const appmodel::HDF5FileLayoutParams& conf)
{
  hdf5libs::HDF5FileLayoutParameters params;
  params.record_name_prefix = conf.get_record_name_prefix();
  params.digits_for_record_number = conf.get_digits_for_record_number();
  params.digits_for_sequence_number = conf.get_digits_for_sequence_number();
  params.record_header_dataset_name = conf.get_record_header_dataset_name();
  params.raw_data_group_name = conf.get_raw_data_group_name();
  params.view_group_name = conf.get_view_group_name();

  for (auto path_conf : conf.get_path_params_list()) {
    hdf5libs::HDF5PathParameters path_params;
    path_params.detector_group_type = path_conf->get_detector_group_type();
    path_params.detector_group_name = path_conf->get_detector_group_name();
    path_params.element_name_prefix = path_conf->get_element_name_prefix();
    path_params.digits_for_element_number = path_conf->get_digits_for_element_number();
    params.path_params_list.push_back(path_params);
  }
  return params;
}

} // namespace dunedaq::dfmodules

#endif // DFMODULES_INCLUDE_DFMODULES_RAWDATAFILECONFIG_HPP_
