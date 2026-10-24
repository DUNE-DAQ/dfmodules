/**
 * @file HDF5FileConfig.hpp Translating HDF5-specific configuration objects for hdf5libs
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_INCLUDE_DFMODULES_HDF5FILECONFIG_HPP_
#define DFMODULES_INCLUDE_DFMODULES_HDF5FILECONFIG_HPP_

#include "hdf5libs/HDF5FileLayoutParameters.hpp"

#include "appmodel/HDF5FileLayoutParams.hpp"
#include "appmodel/HDF5PathParams.hpp"

namespace dunedaq::dfmodules {

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

#endif // DFMODULES_INCLUDE_DFMODULES_HDF5FILECONFIG_HPP_
