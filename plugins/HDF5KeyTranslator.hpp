/**
 * @file HDF5KeyTranslator.hpp
 *
 * HDF5KeyTranslator is a collection of routines to translate between
 * StorageKeys+configuration and HDF5 Group/DataSet 'paths', and between Storage
 * StorageKeys+configuration and HDF5 file names.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_HDF5KEYTRANSLATOR_HPP_
#define DFMODULES_PLUGINS_HDF5KEYTRANSLATOR_HPP_

#include "dfmodules/HDF5FormattingParameters.hpp"
#include "dfmodules/StorageKey.hpp"
#include "dfmodules/hdf5datastore/Nljs.hpp"
#include "dfmodules/hdf5datastore/Structs.hpp"

#include "boost/algorithm/string.hpp"

#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

class HDF5KeyTranslator
{

public:
  inline static const std::string path_separator = "/";

  HDF5KeyTranslator(/*OperationalEnvironmentType op_env_type*/)
  {
    m_data_record_params = HDF5FormattingParameters::get_data_record_parameters(/*op_env_type,*/ s_current_version);
    m_path_param_map = HDF5FormattingParameters::get_path_parameters(/*op_env_type,*/ s_current_version);
  }

  /**
   * @brief Translates the specified StorageKey into an HDF5 'path',
   * where the 'path' is string that has values from the StorageKey
   * separated by slashes (e.g. "<eventID/geoLoc>", "0001/003", "0001/TPC/003").
   * The intention of this path string is to specify the Group/DataSet
   * structure that should be used in the HDF5 files that are created by this library.
   */
  std::string get_path_string(const StorageKey& data_key)
  {
    std::vector<std::string> path_list = get_path_elements(data_key);

    std::string path = path_list[0]; // needs error checking

    for (size_t idx = 1; idx < path_list.size(); ++idx) {
      path = path + path_separator + path_list[idx];
    }

    return path;
  }

  /**
   * @brief Translates the specified StorageKey into the elements of an HDF5 'path',
   * where the 'path' elements are the strings that specify the Group/DataSet
   * structure that should be used in the HDF5 files that are created by this library.
   */
  std::vector<std::string> get_path_elements(const StorageKey& data_key)
  {
    std::vector<std::string> path_list;

    // add trigger number to the path
    std::ostringstream trigger_number_string;
    trigger_number_string << m_data_record_params.trigger_record_name_prefix
                          << std::setw(m_data_record_params.digits_for_trigger_number) << std::setfill('0')
                          << data_key.get_trigger_number();
    path_list.push_back(trigger_number_string.str());

    if (data_key.get_group_type() != StorageKey::DataRecordGroupType::kTriggerRecordHeader) {
      // Add group type
      path_list.push_back(m_path_param_map[data_key.get_group_type()].group_name_within_data_record);

      // next, we translate the region number
      std::ostringstream region_number_string;
      region_number_string << m_path_param_map[data_key.get_group_type()].region_name_prefix
                           << std::setw(m_path_param_map[data_key.get_group_type()].digits_for_region_number)
                           << std::setfill('0') << data_key.get_region_number();
      path_list.push_back(region_number_string.str());

      // Finally, add element number
      std::ostringstream element_number_string;
      element_number_string << m_path_param_map[data_key.get_group_type()].element_name_prefix
                            << std::setw(m_path_param_map[data_key.get_group_type()].digits_for_element_number)
                            << std::setfill('0') << data_key.get_element_number();
      path_list.push_back(element_number_string.str());
    } else {
      // Add TriggerRecordHeader instead of group type
      path_list.push_back("TriggerRecordHeader");
    }

    return path_list;
  }

  /**
   * @brief Returns the version number of the HDF5 paths that are currently being
   * returned by this class. This is independent of the translations from HDF5 paths
   * to StorageKeys (that translation may support multiple versions).
   */
  int get_current_version() { return s_current_version; }

  /**
   * @brief Translates the specified input parameters into the appropriate filename.
   */
  std::string get_file_name(const StorageKey& data_key,
                            const hdf5datastore::ConfParams& config_params,
                            size_t file_index)
  {
    std::ostringstream work_oss;
    work_oss << config_params.directory_path;
    if (work_oss.str().length() > 0) {
      work_oss << "/";
    }
    work_oss << config_params.filename_parameters.overall_prefix;
    if (work_oss.str().length() > 0) {
      work_oss << "_";
    }

    size_t trigger_number = data_key.get_trigger_number();
    size_t region_number = data_key.get_region_number();
    std::string file_name = std::string("");
    if (config_params.mode == "one-event-per-file") {

      file_name = config_params.directory_path + "/" + config_params.filename_parameters.overall_prefix +
                  "_trigger_number_" + std::to_string(trigger_number) + ".hdf5";
      return file_name;

    } else if (config_params.mode == "one-fragment-per-file") {

      file_name = config_params.directory_path + "/" + config_params.filename_parameters.overall_prefix +
                  "_trigger_number_" + std::to_string(trigger_number) + "_region_number_" +
                  std::to_string(region_number) + ".hdf5";
      return file_name;

    } else if (config_params.mode == "all-per-file") {

      // file_name = config_params.directory_path + "/" + config_params.filename_parameters.overall_prefix +
      // "_all_events" + ".hdf5"; file_name = config_params.directory_path + "/" +
      // config_params.filename_parameters.overall_prefix + "_trigger_number_" + "file_number_" +
      // std::to_string(file_index) + ".hdf5";

      work_oss << config_params.filename_parameters.run_number_prefix;
      work_oss << std::setw(config_params.filename_parameters.digits_for_run_number) << std::setfill('0')
               << data_key.get_run_number();
      work_oss << "_";
      work_oss << config_params.filename_parameters.file_index_prefix;
      work_oss << std::setw(config_params.filename_parameters.digits_for_file_index) << std::setfill('0') << file_index;
    }

    work_oss << ".hdf5";
    return work_oss.str();
  }

private:
  static constexpr int s_current_version = 1;
  HDF5FormattingParameters::DataRecordParameters m_data_record_params;
  std::map<StorageKey::DataRecordGroupType, HDF5FormattingParameters::PathParameters> m_path_param_map;
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_HDF5KEYTRANSLATOR_HPP_
