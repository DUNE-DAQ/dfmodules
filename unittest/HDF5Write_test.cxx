/**
 * @file HDF5Write_test.cxx Application that tests and demonstrates
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

#define BOOST_TEST_MODULE HDF5Write_test // NOLINT

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

BOOST_AUTO_TEST_SUITE(HDF5Write_test)

BOOST_AUTO_TEST_CASE(WriteFragmentFiles)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid());

  const int dummydata_size = 7;
  const int run_number = 52;
  const int trigger_count = 5;
  const std::string detector_name = "TPC";
  const int apa_count = 3;
  const int link_count = 1;

  // delete any pre-existing files so that we start with a clean slate
  // std::string delete_pattern = file_prefix + ".*.hdf5";
  std::string delete_pattern = ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  // nlohmann::json conf ;
  // conf["name"] = "tempWriter" ;
  // conf["filename_prefix"] = file_prefix ;
  // conf["directory_path"] = file_path ;
  // conf["mode"] = "one-fragment-per-file" ;

  // create the DataStore
  nlohmann::json conf;
  conf["name"] = "tempWriter";
  conf["directory_path"] = file_path;
  conf["mode"] = "one-fragment-per-file";
  nlohmann::json subconf;
  subconf["overall_prefix"] = file_prefix;
  conf["filename_parameters"] = subconf;
  std::unique_ptr<HDF5DataStore> data_store_ptr(new HDF5DataStore(conf));

  // write several events, each with several fragments
  char dummy_data[dummydata_size];
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number) {
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
  // std::string search_pattern = file_prefix+"_trigger_number*_apa_number_*.hdf5";
  std::string search_pattern = ".*.hdf5";
  std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), (trigger_count * apa_count));

  // clean up the files that were created
  file_list = delete_files_matching_pattern(file_path, delete_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), (trigger_count * apa_count));
}

BOOST_AUTO_TEST_CASE(WriteEventFiles)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid());

  const int dummydata_size = 7;
  const int run_number = 52;
  const int trigger_count = 5;
  const std::string detector_name = "TPC";
  const int apa_count = 3;
  const int link_count = 1;

  // delete any pre-existing files so that we start with a clean slate
  // std::string delete_pattern = file_prefix + ".*.hdf5";
  std::string delete_pattern = ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  // nlohmann::json conf ;
  // conf["name"] = "tempWriter" ;
  // conf["filename_prefix"] = file_prefix ;
  // conf["directory_path"] = file_path ;
  // conf["mode"] = "one-event-per-file" ;
  // std::unique_ptr<HDF5DataStore> data_store_ptr(new HDF5DataStore(conf));

  // create the DataStore
  nlohmann::json conf;
  conf["name"] = "tempWriter";
  conf["directory_path"] = file_path;
  conf["mode"] = "one-event-per-file";
  nlohmann::json subconf;
  subconf["overall_prefix"] = file_prefix;
  conf["filename_parameters"] = subconf;
  std::unique_ptr<HDF5DataStore> data_store_ptr(new HDF5DataStore(conf));

  // write several events, each with several fragments
  char dummy_data[dummydata_size];
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number) {
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
  std::string search_pattern = ".*.hdf5";
  // std::string search_pattern = file_prefix + "trigger_number*.hdf5";
  std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), trigger_count);

  // clean up the files that were created
  file_list = delete_files_matching_pattern(file_path, delete_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), trigger_count);
}

BOOST_AUTO_TEST_CASE(WriteOneFile)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid());

  const int dummydata_size = 7;
  const int run_number = 52;
  const int trigger_count = 5;
  const std::string detector_name = "TPC";
  const int apa_count = 3;
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

BOOST_AUTO_TEST_CASE(FileSizeLimitResultsInMultipleFiles)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid());

  const int dummydata_size = 10000;
  const int run_number = 52;
  const int trigger_count = 15;
  const std::string detector_name = "TPC";
  const int apa_count = 5;
  const int link_count = 10;

  // 5 APAs times 10 links time 10000 bytes per fragment gives 500,000 bytes per TR
  // So, 15 TRs would give 7,500,000 bytes total.

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  nlohmann::json conf;
  conf["name"] = "tempWriter";
  conf["directory_path"] = file_path;
  conf["mode"] = "all-per-file";
  conf["max_file_size_bytes"] = 3000000;
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

  char dummy_data[dummydata_size];
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number) {
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
  // 7,500,000 bytes stored in files of size 3,000,000 should result in three files.
  BOOST_REQUIRE_EQUAL(file_list.size(), 3);

  // clean up the files that were created
  file_list = delete_files_matching_pattern(file_path, delete_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), 3);
}

BOOST_AUTO_TEST_SUITE_END()
