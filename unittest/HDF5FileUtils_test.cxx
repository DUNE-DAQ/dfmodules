/**
 * @file HDF5FileUtils_test.cxx Test application that tests and demonstrates
 * the functionality of the HDF5FileUtils class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "HDF5FileUtils.hpp"

#include "ers/ers.h"

#define BOOST_TEST_MODULE HDF5FileUtils_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

using namespace dunedaq::dfmodules;

std::vector<std::string>
delete_files_matching_pattern(const std::string& path, const std::string& pattern)
{
  std::regex regex_search_pattern(pattern);
  std::vector<std::string> file_list;
  for (const auto& entry : std::filesystem::directory_iterator(path)) {
    if (std::regex_match(entry.path().filename().string(), regex_search_pattern)) {
      if (std::filesystem::remove(entry.path())) {
        file_list.push_back(entry.path());
      }
    }
  }
  return file_list;
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
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "kurt";
  std::string file_extension = ".tmp";
  std::string pid = std::to_string(getpid());

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*" + pid + ".*" + file_extension;
  delete_files_matching_pattern(file_path, delete_pattern);

  // create a few test files
  std::string fullPath;
  fullPath = file_path + "/" + file_prefix + "_1_" + pid + file_extension;
  touchFile(fullPath);
  fullPath = file_path + "/" + file_prefix + "_2_" + pid + file_extension;
  touchFile(fullPath);
  fullPath = file_path + "/" + file_prefix + "_3_" + pid + file_extension;
  touchFile(fullPath);

  std::string search_pattern = file_prefix + ".*" + pid + ".*" + file_extension;
  std::vector<std::string> file_list = HDF5FileUtils::get_files_matching_pattern(file_path, search_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), 3);

  file_list = delete_files_matching_pattern(file_path, delete_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), 3);
}

BOOST_AUTO_TEST_SUITE_END()
