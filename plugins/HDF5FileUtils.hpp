#ifndef DDPDEMO_SRC_HDF5FILEUTILS_HPP_
#define DDPDEMO_SRC_HDF5FILEUTILS_HPP_
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

#include <ers/ers.h>
#include <highfive/H5File.hpp>

#include <filesystem>
#include <regex>
#include <string>
#include <vector>

//#include "ddpdemo/StorageKey.hpp"
//#include <boost/algorithm/string.hpp>
//#include <iomanip>
//#include <sstream>

namespace dunedaq {
namespace ddpdemo {

namespace HDF5FileUtils {

/**
 * @brief This is a recursive function that adds the 'paths' to all of the DataSets
 * contained within the specified Group to the specified path list.  This function
 * is used by the getAlDataSetPaths() function.
 */
void
addDataSetsToPath(HighFive::Group parentGroup, const std::string& parentPath, std::vector<std::string>& pathList)
{
  std::vector<std::string> childNames = parentGroup.listObjectNames();
  for (auto& childName : childNames) {
    std::string fullPath = parentPath + "/" + childName;
    HighFive::ObjectType childType = parentGroup.getObjectType(childName);
    if (childType == HighFive::ObjectType::Dataset) {
      pathList.push_back(fullPath);
    } else if (childType == HighFive::ObjectType::Group) {
      HighFive::Group childGroup = parentGroup.getGroup(childName);
      addDataSetsToPath(childGroup, fullPath, pathList);
    }
  }
}

/**
 * @brief Fetches the list of all DataSet paths in the specified file.
 */
std::vector<std::string>
getAllDataSetPaths(const HighFive::File& hdfFile)
{
  std::vector<std::string> pathList;

  std::vector<std::string> topLevelNames = hdfFile.listObjectNames();
  for (auto& topLevelName : topLevelNames) {
    HighFive::ObjectType topLevelType = hdfFile.getObjectType(topLevelName);
    // ERS_INFO("Top level name and type: " << topLevelName << " " << ((int)topLevelType));
    if (topLevelType == HighFive::ObjectType::Dataset) {
      pathList.push_back(topLevelName);
    } else if (topLevelType == HighFive::ObjectType::Group) {
      HighFive::Group topLevelGroup = hdfFile.getGroup(topLevelName);
      addDataSetsToPath(topLevelGroup, topLevelName, pathList);
    }
  }

  return pathList;
}

/**
 * @brief Fetches the list of files in the specified directory that have
 * filenames that match the specified search pattern.  The search pattern uses regex
 * syntax (e.g. ".*" to match zero or more instances of any character).
 * @return the list of filenames
 */
std::vector<std::string>
getFilesMatchingPattern(const std::string& directoryPath, const std::string& filenamePattern)
{
  std::regex regexSearchPattern(filenamePattern);
  std::vector<std::string> fileList;
  for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
    // TLOG(TLVL_DEBUG) << "Directory element: " << entry.path().string();
    if (std::regex_match(entry.path().filename().string(), regexSearchPattern)) {
      // TLOG(TLVL_DEBUG) << "Matching directory element: " << entry.path().string();
      fileList.push_back(entry.path());
    }
  }
  return fileList;
}

} // namespace HDF5FileUtils

} // namespace ddpdemo
} // namespace dunedaq

#endif // DDPDEMO_SRC_HDF5FILEUTILS_HPP_
