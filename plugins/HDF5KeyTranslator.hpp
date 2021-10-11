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

#include "dfmodules/StorageKey.hpp"
#include "dfmodules/hdf5datastore/Nljs.hpp"
#include "dfmodules/hdf5datastore/Structs.hpp"

#include "logging/Logging.hpp"

#include "boost/algorithm/string.hpp"

#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace hdf5ds = dunedaq::dfmodules::hdf5datastore;

namespace dunedaq {

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidHDF5GroupTypeConfigParams,
                       appfwk::GeneralDAQModuleIssue,
                       "Invalid detector group type (\"" << group_type
                       << "\") found in the configuration of the HDF5 internal layout.",
                       ((std::string)name),
                       ((std::string)group_type))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       RequestedHDF5GroupTypeNotFound,
                       appfwk::GeneralDAQModuleIssue,
                       "Invalid detector group type (" << group_type
                       << ") requested when attempting to determine the HDF5 Group and DataSet path.",
                       ((std::string)name),
                       ((int)group_type))

namespace dfmodules {

class HDF5KeyTranslator
{

public:
  inline static const std::string path_separator = "/";

  struct DataRecordParameters
  {
    std::string trigger_record_name_prefix;
    int digits_for_trigger_number;
  };

  HDF5KeyTranslator(const hdf5datastore::ConfParams& config_params)
  {
    m_data_record_params.trigger_record_name_prefix = config_params.file_layout_parameters.trigger_record_name_prefix;
    m_data_record_params.digits_for_trigger_number = config_params.file_layout_parameters.digits_for_trigger_number;

    m_current_version = config_params.version;

    hdf5ds::PathParamList path_param_list = config_params.file_layout_parameters.path_param_list;
    for (const hdf5ds::PathParams& path_param_set : path_param_list) {
      if (path_param_set.detector_group_type == "TPC") {
        m_path_param_map[StorageKey::DataRecordGroupType::kTPC] = path_param_set;
      }
      else if (path_param_set.detector_group_type == "PDS") {
        m_path_param_map[StorageKey::DataRecordGroupType::kPDS] = path_param_set;
      }
      else if (path_param_set.detector_group_type == "Trigger") {
        m_path_param_map[StorageKey::DataRecordGroupType::kTrigger] = path_param_set;
      }
      else if (path_param_set.detector_group_type == "TPC_TP") {
        m_path_param_map[StorageKey::DataRecordGroupType::kTPC_TP] = path_param_set;
      }
      else {
        throw InvalidHDF5GroupTypeConfigParams(ERS_HERE, "HDF5KeyTranslator", path_param_set.detector_group_type);
      }
    }

    m_config_params = config_params;
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

    // verify that the requested detector group type is in our map of parameters
    if (data_key.get_group_type() != StorageKey::DataRecordGroupType::kTriggerRecordHeader &&
        m_path_param_map.count(data_key.get_group_type()) == 0) {
      throw RequestedHDF5GroupTypeNotFound(ERS_HERE, "HDF5KeyTranslator", data_key.get_group_type());
    }

    // add trigger number to the path
    std::ostringstream trigger_number_string;
    trigger_number_string << m_data_record_params.trigger_record_name_prefix
                          << std::setw(m_data_record_params.digits_for_trigger_number) << std::setfill('0')
                          << data_key.get_trigger_number();
    if (data_key.m_max_sequence_number > 0) {
      trigger_number_string << "." << data_key.m_this_sequence_number;
    }
    path_list.push_back(trigger_number_string.str());

    if (data_key.get_group_type() != StorageKey::DataRecordGroupType::kTriggerRecordHeader) {
      // Add group type
      path_list.push_back(m_path_param_map[data_key.get_group_type()].detector_group_name);

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
   * returned by this class.
   */
  int get_current_version() { return m_current_version; }

  /**
   * @brief Translates the specified input parameters into the appropriate filename.
   */
  std::string get_file_name(const StorageKey& data_key,
                            size_t file_index)
  {
    std::ostringstream work_oss;
    work_oss << m_config_params.directory_path;
    if (work_oss.str().length() > 0) {
      work_oss << "/";
    }
    work_oss << m_config_params.filename_parameters.overall_prefix;
    if (work_oss.str().length() > 0) {
      work_oss << "_";
    }

    size_t trigger_number = data_key.get_trigger_number();
    size_t region_number = data_key.get_region_number();
    std::string file_name = std::string("");
    if (m_config_params.mode == "one-event-per-file") {

      file_name = m_config_params.directory_path + "/" + m_config_params.filename_parameters.overall_prefix +
                  "_trigger_number_" + std::to_string(trigger_number) + ".hdf5";
      return file_name;

    } else if (m_config_params.mode == "one-fragment-per-file") {

      file_name = m_config_params.directory_path + "/" + m_config_params.filename_parameters.overall_prefix +
                  "_trigger_number_" + std::to_string(trigger_number) + "_region_number_" +
                  std::to_string(region_number) + ".hdf5";
      return file_name;

    } else if (m_config_params.mode == "all-per-file") {

      // file_name = m_config_params.directory_path + "/" + m_config_params.filename_parameters.overall_prefix +
      // "_all_events" + ".hdf5"; file_name = m_config_params.directory_path + "/" +
      // m_config_params.filename_parameters.overall_prefix + "_trigger_number_" + "file_number_" +
      // std::to_string(file_index) + ".hdf5";

      work_oss << m_config_params.filename_parameters.run_number_prefix;
      work_oss << std::setw(m_config_params.filename_parameters.digits_for_run_number) << std::setfill('0')
               << data_key.get_run_number();
      work_oss << "_";
      work_oss << m_config_params.filename_parameters.file_index_prefix;
      work_oss << std::setw(m_config_params.filename_parameters.digits_for_file_index) << std::setfill('0') << file_index;
    }

    work_oss << ".hdf5";
    return work_oss.str();
  }

private:
  int m_current_version = 1;
  DataRecordParameters m_data_record_params;
  hdf5datastore::ConfParams m_config_params;
  std::map<StorageKey::DataRecordGroupType, hdf5ds::PathParams> m_path_param_map;
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_HDF5KEYTRANSLATOR_HPP_
