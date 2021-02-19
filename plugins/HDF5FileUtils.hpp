/**
 * @file HDF5FileUtils.hpp
 *
 * HDF5FileUtils collection of functions to assist with interacting with
 * HDF5 files.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_HDF5FILEUTILS_HPP_
#define DFMODULES_PLUGINS_HDF5FILEUTILS_HPP_

#include "dfmodules/CommonIssues.hpp"
#include "logging/Logging.hpp"

#include "highfive/H5File.hpp"

#include <filesystem>
#include <memory>
#include <regex>
#include <string>
#include <vector>

//#include "dfmodules/StorageKey.hpp"
//#include <boost/algorithm/string.hpp>
//#include <iomanip>
//#include <sstream>

namespace dunedaq {
namespace dfmodules {
namespace HDF5FileUtils {
/**
 * @brief Retrieve top HDF5 group
 */
HighFive::Group
get_top_group(std::unique_ptr<HighFive::File>& file_ptr, const std::vector<std::string>& group_dataset)
{
  std::string top_level_group_name = group_dataset[0];
  HighFive::Group top_group = file_ptr->getGroup(top_level_group_name);
  if (!top_group.isValid()) {
    // throw InvalidHDF5Group(ERS_HERE, get_name(), top_level_group_name);
    throw InvalidHDF5Group(ERS_HERE, top_level_group_name, top_level_group_name);
  }

  return top_group;
}

/**
 * @brief Recursive function to create HDF5 sub-groups
 */
HighFive::Group
get_subgroup(std::unique_ptr<HighFive::File>& file_ptr,
             const std::vector<std::string>& group_dataset,
             bool create_if_needed)
{
  std::string top_level_group_name = group_dataset[0];
  if (create_if_needed && !file_ptr->exist(top_level_group_name)) {
    file_ptr->createGroup(top_level_group_name);
  }
  HighFive::Group working_group = file_ptr->getGroup(top_level_group_name);
  if (!working_group.isValid()) {
    throw InvalidHDF5Group(ERS_HERE, top_level_group_name, top_level_group_name);
  }
  // Create the remaining subgroups
  for (size_t idx = 1; idx < group_dataset.size() - 1; ++idx) {
    // group_dataset.size()-1 because the last element is the dataset
    std::string child_group_name = group_dataset[idx];
    if (child_group_name.empty()) {
      throw InvalidHDF5Group(ERS_HERE, child_group_name, child_group_name);
    }
    if (create_if_needed && !working_group.exist(child_group_name)) {
      working_group.createGroup(child_group_name);
    }
    HighFive::Group child_group = working_group.getGroup(child_group_name);
    if (!child_group.isValid()) {
      throw InvalidHDF5Group(ERS_HERE, child_group_name, child_group_name);
    }
    working_group = child_group;
  }

  return working_group;
}
/**
 * @brief This is a recursive function that adds the 'paths' to all of the DataSets
 * contained within the specified Group to the specified path list.  This function
 * is used by the getAlDataSetPaths() function.
 */
void
add_datasets_to_path(HighFive::Group parent_group, const std::string& parent_path, std::vector<std::string>& path_list)
{
  std::vector<std::string> childNames = parent_group.listObjectNames();
  for (auto& child_name : childNames) {
    std::string full_path = parent_path + "/" + child_name;
    HighFive::ObjectType child_type = parent_group.getObjectType(child_name);
    if (child_type == HighFive::ObjectType::Dataset) {
      path_list.push_back(full_path);
    } else if (child_type == HighFive::ObjectType::Group) {
      HighFive::Group child_group = parent_group.getGroup(child_name);
      add_datasets_to_path(child_group, full_path, path_list);
    }
  }
}

/**
 * @brief Fetches the list of all DataSet paths in the specified file.
 */
std::vector<std::string>
get_all_dataset_paths(const HighFive::File& hdf_file)
{
  std::vector<std::string> path_list;

  std::vector<std::string> top_level_names = hdf_file.listObjectNames();
  for (auto& top_level_name : top_level_names) {
    HighFive::ObjectType top_level_type = hdf_file.getObjectType(top_level_name);
    // TLOG("HDF5FileUtils") << "Top level name and type: " << top_level_name << " " << ((int)top_level_type);
    if (top_level_type == HighFive::ObjectType::Dataset) {
      path_list.push_back(top_level_name);
    } else if (top_level_type == HighFive::ObjectType::Group) {
      HighFive::Group top_level_group = hdf_file.getGroup(top_level_name);
      add_datasets_to_path(top_level_group, top_level_name, path_list);
    }
  }

  return path_list;
}

/**
 * @brief Fetches the list of files in the specified directory that have
 * filenames that match the specified search pattern.  The search pattern uses regex
 * syntax (e.g. ".*" to match zero or more instances of any character).
 * @return the list of filenames
 */
std::vector<std::string>
get_files_matching_pattern(const std::string& directory_path, const std::string& filename_pattern)
{
  std::regex regexSearchPattern(filename_pattern);
  std::vector<std::string> file_list;
  for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
    // TLOG("HDF5FileUtils") << "Directory element: " << entry.path().string();
    if (std::regex_match(entry.path().filename().string(), regexSearchPattern)) {
      // TLOG("HDF5FileUtils") << "Matching directory element: " << entry.path().string();
      file_list.push_back(entry.path());
    }
  }
  return file_list;
}

} // namespace HDF5FileUtils

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_HDF5FILEUTILS_HPP_
