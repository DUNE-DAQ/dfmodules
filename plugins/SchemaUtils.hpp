
#include "dfmodules/hdf5datastore/Structs.hpp"
#include "hdf5libs/hdf5filelayout/Structs.hpp"
#include "hdf5libs/hdf5rawdatafile/Structs.hpp"

#include "appdal/DataStoreConf.hpp"
#include "appdal/FilenameParams.hpp"
#include "appdal/HDF5FileLayoutParams.hpp"
#include "appdal/HDF5PathParams.hpp"
#include "coredal/DROStreamConf.hpp"
#include "coredal/GeoId.hpp"
#include "coredal/DetectorConfig.hpp"
#include "coredal/ReadoutGroup.hpp"
#include "coredal/ReadoutMap.hpp"

namespace dunedaq::dfmodules {

hdf5libs::hdf5rawdatafile::GeoID
convert_to_json(const coredal::GeoId* geoid)
{
  hdf5libs::hdf5rawdatafile::GeoID output;

  output.det_id = geoid->get_detector_id();
  output.crate_id = geoid->get_crate_id();
  output.slot_id = geoid->get_slot_id();
  output.stream_id = geoid->get_stream_id();

  return output;
}

hdf5libs::hdf5rawdatafile::SrcIDGeoIDMap
convert_to_json(const coredal::ReadoutGroup* group)
{

  hdf5libs::hdf5rawdatafile::SrcIDGeoIDMap output;

  // Do we want to check for disabled groups?

  for (auto res : group->get_contains()) {
    hdf5libs::hdf5rawdatafile::SrcIDGeoIDEntry entry;
    auto stream = res->cast<coredal::DROStreamConf>();

    entry.src_id = stream->get_src_id();
    entry.geo_id = convert_to_json(stream->get_geo_id());

    output.push_back(entry);
  }

  return output;
}

hdf5libs::hdf5rawdatafile::SrcIDGeoIDMap
convert_to_json(const coredal::ReadoutMap* map)
{
  hdf5libs::hdf5rawdatafile::SrcIDGeoIDMap output;

  for (auto group : map->get_groups()) {
    auto groupMap = convert_to_json(group);
    output.insert(output.end(), groupMap.begin(), groupMap.end());
  }

  return output;
}
hdf5libs::hdf5filelayout::PathParams
convert_to_json(const appdal::HDF5PathParams* params)
{
  hdf5libs::hdf5filelayout::PathParams output;

  output.detector_group_type = params->get_detector_group_type();
  output.detector_group_name = params->get_detector_group_name();
  output.element_name_prefix = params->get_element_name_prefix();
  output.digits_for_element_number = params->get_digits_for_element_number();

  return output;
}
hdf5libs::hdf5filelayout::FileLayoutParams
convert_to_json(const appdal::HDF5FileLayoutParams* params)
{
  hdf5libs::hdf5filelayout::FileLayoutParams output;

  output.record_name_prefix = params->get_record_name_prefix();
  output.digits_for_record_number = params->get_digits_for_record_number();
  output.digits_for_sequence_number = params->get_digits_for_sequence_number();
  output.record_header_dataset_name = params->get_record_header_dataset_name();
  output.raw_data_group_name = params->get_raw_data_group_name();
  output.view_group_name = params->get_view_group_name();

  for (auto& path_param : params->get_path_params_list()) {
    output.path_param_list.push_back(convert_to_json(path_param));
  }

  return output;
}

hdf5datastore::FileNameParams
convert_to_json(const appdal::FilenameParams* params)
{
  hdf5datastore::FileNameParams output;
  output.overall_prefix = params->get_overall_prefix();
  output.run_number_prefix = params->get_run_number_prefix();
  output.digits_for_run_number = params->get_digits_for_run_number();
  output.file_index_prefix = params->get_file_index_prefix();
  output.digits_for_file_index = params->get_digits_for_file_index();
  output.writer_identifier = params->get_writer_identifier();
  output.trigger_number_prefix = params->get_trigger_number_prefix();
  output.digits_for_trigger_number = params->get_digits_for_trigger_number();

  return output;
}

hdf5datastore::ConfParams
convert_to_json(const appdal::DataStoreConf* params, const coredal::ReadoutMap* readout_map, const coredal::DetectorConfig* det_conf)
{
  hdf5datastore::ConfParams output;
  output.type = params->get_type();
  output.name = params->UID();
  output.operational_environment = det_conf->get_op_env();
  output.mode = params->get_mode();
  output.directory_path = params->get_directory_path();
  output.max_file_size_bytes = params->get_max_file_size();
  output.disable_unique_filename_suffix = params->get_disable_unique_filename_suffix();
  output.filename_parameters = convert_to_json(params->get_filename_params());
  output.file_layout_parameters = convert_to_json(params->get_file_layout_params());
  output.free_space_safety_factor_for_write = params->get_free_space_safety_factor();

  output.srcid_geoid_map = convert_to_json(readout_map);

  return output;
}

}