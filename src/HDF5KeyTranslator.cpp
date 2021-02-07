/**
 * @file HDF5KeyTranslator.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/HDF5KeyTranslator.hpp"

namespace dunedaq {
namespace dfmodules {

std::string
HDF5KeyTranslator::get_path_string(const StorageKey& data_key,
                                   const hdf5datastore::HDF5DataStoreFileLayoutParams& layout_params)
{
  std::vector<std::string> element_list = get_path_elements(data_key, layout_params);

  std::string path = element_list[0]; // need error checking

  for (size_t idx = 1; idx < element_list.size(); ++idx) {
    path = path + path_separator + element_list[idx];
  }

  return path;
}

} // namespace dfmodules
} // namespace dunedaq
