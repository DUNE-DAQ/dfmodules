/**
 * @file HDF5FileUtils_test.cxx Test application that tests and demonstrates
 * the functionality of the HDF5FileUtils class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "../plugins/HDF5FileUtils.hpp"

#include "ers/ers.h"

#define BOOST_TEST_MODULE HDF5FileUtils_test // NOLINT

#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

using namespace dunedaq::ddpdemo;

std::vector<std::string>
deleteFilesMatchingPattern(const std::string& path, const std::string& pattern)
{
  std::regex regexSearchPattern(pattern);
  std::vector<std::string> fileList;
  for (const auto& entry : std::filesystem::directory_iterator(path)) {
    if (std::regex_match(entry.path().filename().string(), regexSearchPattern)) {
      if (std::filesystem::remove(entry.path())) {
        fileList.push_back(entry.path());
      }
    }
  }
  return fileList;
}

void
touchFile(const std::string& filepath)
{
  std::fstream fs;
  fs.open(filepath, std::ios::out);
  fs.close();
}

BOOST_AUTO_TEST_SUITE(HDF5FileUtils_test)

BOOST_AUTO_TEST_CASE(GetFileList)
{
  std::string filePath(std::filesystem::temp_directory_path());
  std::string filePrefix = "kurt";
  std::string fileExtension = ".tmp";
  std::string pid = std::to_string(getpid());

  // delete any pre-existing files so that we start with a clean slate
  std::string deletePattern = filePrefix + ".*" + pid + ".*" + fileExtension;
  deleteFilesMatchingPattern(filePath, deletePattern);

  // create a few test files
  std::string fullPath;
  fullPath = filePath + "/" + filePrefix + "_1_" + pid + fileExtension;
  touchFile(fullPath);
  fullPath = filePath + "/" + filePrefix + "_2_" + pid + fileExtension;
  touchFile(fullPath);
  fullPath = filePath + "/" + filePrefix + "_3_" + pid + fileExtension;
  touchFile(fullPath);

  std::string searchPattern = filePrefix + ".*" + pid + ".*" + fileExtension;
  std::vector<std::string> fileList = HDF5FileUtils::getFilesMatchingPattern(filePath, searchPattern);
  BOOST_REQUIRE_EQUAL(fileList.size(), 3);

  fileList = deleteFilesMatchingPattern(filePath, deletePattern);
  BOOST_REQUIRE_EQUAL(fileList.size(), 3);
}

BOOST_AUTO_TEST_SUITE_END()
