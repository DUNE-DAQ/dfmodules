/**
 * @file TriggerRecordHeaderWrite_test.cxx Application that tests and demonstrates
 * the write functionality of the HDF5DataStore class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

//#include "dfmodules/DataStore.hpp"
//#include "dfmodules/hdf5datastore/Nljs.hpp"
//#include "dfmodules/hdf5datastore/Structs.hpp"
#include "HDF5DataStore.hpp"

#define BOOST_TEST_MODULE TriggerRecordHeaderWrite_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <vector>

using namespace dunedaq::dfmodules;

std::vector<std::string>
get_files_matching_pattern(const std::string& path, const std::string& pattern)
{
  std::regex regex_search_pattern(pattern);
  std::vector<std::string> file_list;
  for (const auto& entry : std::filesystem::directory_iterator(path)) {
    if (std::regex_match(entry.path().filename().string(), regex_search_pattern)) {
      file_list.push_back(entry.path());
    }
  }
  return file_list;
}

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

BOOST_AUTO_TEST_SUITE(TriggerRecordHeaderWrite_test)

BOOST_AUTO_TEST_CASE(WriteOneFile)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid());

  const int dummydata_size = 7;
  const int run_number = 52;
  const int trigger_count = 1;
  const std::string detector_name = "TPC";
  const int apa_count = 1;
  const int link_count = 1;

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  nlohmann::json conf;
  conf["name"] = "tempWriter";
  conf["directory_path"] = file_path;
  conf["mode"] = "all-per-file";
  nlohmann::json subconf;
  subconf["overall_prefix"] = file_prefix;
  conf["filename_parameters"] = subconf;
  std::unique_ptr<HDF5DataStore> data_store_ptr(new HDF5DataStore(conf));
#if 0
  hdf5datastore::ConfParams config_params;
  config_params.name = "tempWriter";
  config_params.mode = "all-per-file";
  config_params.max_file_size_bytes = 10000000;  // much larger than what we expect, so no second file;
  config_params.directory_path = file_path;
  config_params.filename_parameters.overall_prefix = file_prefix;
  hdf5datastore::data_t hdf5ds_json;
  hdf5datastore::to_json(hdf5ds_json, config_params);
  std::unique_ptr<DataStore> data_store_ptr;
  data_store_ptr = make_data_store(hdf5ds_json);
#endif

  // write several events, each with several fragments
  char dummy_data[dummydata_size];
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number) {

    // TriggerRecordHeader
    StorageKey trh_key(run_number, trigger_number, "TriggerRecordHeader", 1, 1);
    KeyedDataBlock trh_data_block(trh_key);
    trh_data_block.m_unowned_data_start = static_cast<void*>(&dummy_data[0]);
    trh_data_block.m_data_size = 1;
    data_store_ptr->write(trh_data_block);

    // Write fragments
    for (int apa_number = 1; apa_number <= apa_count; ++apa_number) {
      for (int link_number = 1; link_number <= link_count; ++link_number) {
        StorageKey key(run_number, trigger_number, detector_name, apa_number, link_number);
        KeyedDataBlock data_block(key);
        data_block.m_unowned_data_start = static_cast<void*>(&dummy_data[0]);
        data_block.m_data_size = dummydata_size;
        data_store_ptr->write(data_block);
      }                   // link number
    }                     // apa number
  }                       // trigger number
  data_store_ptr.reset(); // explicit destruction

  // check that the expected number of files was created
  std::string search_pattern = file_prefix + ".*.hdf5";
  std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);

  // clean up the files that were created
  file_list = delete_files_matching_pattern(file_path, delete_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);
}
BOOST_AUTO_TEST_SUITE_END()
