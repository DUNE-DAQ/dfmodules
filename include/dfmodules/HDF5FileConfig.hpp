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

inline hdf5libs::HDF5FileLayoutParameters
make_hdf5_file_layout_parameters(const appmodel::HDF5FileLayoutParams& conf);

} // namespace dunedaq::dfmodules

#include "detail/HDF5FileConfig.hxx"

#endif // DFMODULES_INCLUDE_DFMODULES_HDF5FILECONFIG_HPP_
