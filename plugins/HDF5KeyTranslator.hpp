/**
 * @file HDF5KeyTranslator.hpp
 *
 * HDF5KeyTranslator is a collection of functions to translate between
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

#include <boost/algorithm/string.hpp>

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

class HDF5KeyTranslator
{

public:
  inline static const std::string path_separator = "/";

  /**
   * @brief Translates the specified StorageKey into an HDF5 'path',
   * where the 'path' is string that has values from the StorageKey
   * separated by slashes (e.g. "<eventID/geoLoc>", "0001/003", "0001/FELIX/003").
   * The intention of this path string is to specify the Group/DataSet
   * structure that should be used in the HDF5 files that are created by this library.
   */
  static std::string get_path_string(const StorageKey& data_key,
                                     const hdf5datastore::HDF5DataStoreFileLayoutParams& layout_params)
  {
    std::vector<std::string> element_list = get_path_elements(data_key, layout_params);

    std::string path = element_list[0]; // need error checking

    for (size_t idx = 1; idx < element_list.size(); ++idx) {
      path = path + path_separator + element_list[idx];
    }

    return path;
  }

  /**
   * @brief Translates the specified StorageKey into the elements of an HDF5 'path',
   * where the 'path' elements are the strings that specify the Group/DataSet
   * structure that should be used in the HDF5 files that are created by this library.
   */
  static std::vector<std::string> get_path_elements(const StorageKey& data_key,
                                                    const hdf5datastore::HDF5DataStoreFileLayoutParams& layout_params)
  {
    std::vector<std::string> element_list;

    // first, we take care of the trigger number
    std::ostringstream trigger_number_string;
    trigger_number_string << layout_params.trigger_record_name_prefix
                        << std::setw(layout_params.digits_for_trigger_number) << std::setfill('0')
                        << data_key.get_trigger_number();
    element_list.push_back(trigger_number_string.str());

    if (data_key.get_detector_type() != "TriggerRecordHeader") {
      // Add detector type
      element_list.push_back(data_key.get_detector_type());

      // next, we translate the APA number location
      std::ostringstream apa_number_string;
      apa_number_string << layout_params.apa_name_prefix << std::setw(layout_params.digits_for_apa_number)
                      << std::setfill('0') << data_key.get_apa_number();
      element_list.push_back(apa_number_string.str());

      // Finally, add link number
      std::ostringstream link_number_string;
      link_number_string << layout_params.link_name_prefix << std::setw(layout_params.digits_for_link_number)
                       << std::setfill('0') << data_key.get_link_number();
      element_list.push_back(link_number_string.str());
    } else {
      // Add TriggerRecordHeader instead of detector type
      element_list.push_back("TriggerRecordHeader");
    }

    return element_list;
  }

  /**
   * @brief Returns the version number of the HDF5 paths that are currently being
   * returned by this class. This is independent of the translations from HDF5 paths
   * to StorageKeys (that translation may support multiple versions).
   */
  static int get_current_version() { return current_version; }

  /**
   * @brief Translates the specified HDF5 'path' into the appropriate StorageKey.
   */
  static StorageKey get_key_from_string(const std::string& path, int translation_version = current_version)
  {
    std::vector<std::string> element_list;
    boost::split(element_list, path, boost::is_any_of(path_separator));
    return get_key_from_list(element_list, translation_version);
  }

  /**
   * @brief Translates the specified HDF5 'path' elements into the appropriate StorageKey.
   */
  static StorageKey get_key_from_list(const std::vector<std::string>& path_elements,
                                   int translation_version = current_version)
  {
    if (translation_version == 1) {
      int run_number = StorageKey::s_invalid_run_number;
      int trigger_number = StorageKey::s_invalid_trigger_number;
      std::string detector_type = StorageKey::s_invalid_detector_type;
      int apa_number = StorageKey::s_invalid_apa_number;
      int link_number = StorageKey::s_invalid_link_number;

      if (path_elements.size() >= 1) {
        std::stringstream runNumber(path_elements[0]);
        runNumber >> run_number;
      }
      if (path_elements.size() >= 2) {
        std::stringstream trigNumber(path_elements[1]);
        trigNumber >> trigger_number;
      }

      if (path_elements.size() >= 3) {
        std::stringstream detectorType(path_elements[2]);
        detectorType >> detector_type;
      }

      if (path_elements.size() >= 4) {
        std::stringstream apaNumber(path_elements[3]);
        apaNumber >> apa_number;
      }

      if (path_elements.size() >= 5) {
        std::stringstream linkNumber(path_elements[4]);
        linkNumber >> link_number;
      }

      return StorageKey(run_number, trigger_number, detector_type, apa_number, link_number);

    } else {
      StorageKey emptyKey(StorageKey::s_invalid_run_number,
                          StorageKey::s_invalid_trigger_number,
                          StorageKey::s_invalid_detector_type,
                          StorageKey::s_invalid_apa_number,
                          StorageKey::s_invalid_link_number);
      return emptyKey;
    }
  }

  /**
   * @brief Translates the specified input parameters into the appropriate filename.
   */
  static std::string get_file_name(const StorageKey& data_key,
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
    size_t apa_number = data_key.get_apa_number();
    std::string file_name = std::string("");
    if (config_params.mode == "one-event-per-file") {

      file_name = config_params.directory_path + "/" + config_params.filename_parameters.overall_prefix +
                  "_trigger_number_" + std::to_string(trigger_number) + ".hdf5";
      return file_name;

    } else if (config_params.mode == "one-fragment-per-file") {

      file_name = config_params.directory_path + "/" + config_params.filename_parameters.overall_prefix +
                  "_trigger_number_" + std::to_string(trigger_number) + "_apa_number_" + std::to_string(apa_number) +
                  ".hdf5";
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
  static const int current_version = 1;
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_HDF5KEYTRANSLATOR_HPP_
