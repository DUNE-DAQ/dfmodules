/**
 * @file RawDataFileConfig.hpp Translating configuration objects into what raw data files store
 *
 * Raw data files are interpreted on their own, so the parts of the configuration they need
 * (which GeoIDs feed each SourceID) are copied into them. The translation lives here, with the
 * DataStores, so that the file libraries don't depend on the configuration packages.
 * HDF5-only translations are in HDF5FileConfig.hpp.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_INCLUDE_DFMODULES_RAWDATAFILECONFIG_HPP_
#define DFMODULES_INCLUDE_DFMODULES_RAWDATAFILECONFIG_HPP_

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

#include <cstdint>

namespace dunedaq::dfmodules {

/**
 * @brief The GeoIDs of the detector streams of every enabled readout connection in the session,
 * by the SourceID that each stream's data is sent with.
 */
inline daqdataformats::SourceIDGeoIDMap
make_source_id_geo_id_map(const confmodel::Session& session);

} // namespace dunedaq::dfmodules

#include "detail/RawDataFileConfig.hxx"

#endif // DFMODULES_INCLUDE_DFMODULES_RAWDATAFILECONFIG_HPP_
