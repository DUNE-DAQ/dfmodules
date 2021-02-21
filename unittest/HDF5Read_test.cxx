/**
 * @file HDF5Read_test.cxx Application that tests and demonstrates
 * the read functionality of the HDF5DataStore class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "HDF5DataStore.hpp"

#define BOOST_TEST_MODULE HDF5Read_test // NOLINT

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

BOOST_AUTO_TEST_SUITE(HDF5Read_test)

/*
BOOST_AUTO_TEST_CASE(ReadFragmentFiles)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid());
  const int events_to_generate = 2;
  const int links_to_generate = 2;
  const int dummydata_size = 128;

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore instance for writing
  nlohmann::json conf ;
  conf["name"] = "tempWriter" ;
  conf["filename_prefix"] = file_prefix ;
  conf["directory_path"] = file_path ;
  conf["mode"] = "one-fragment-per-file" ;
  std::unique_ptr<HDF5DataStore> data_store_ptr(new HDF5DataStore( conf ));

  // write several events, each with several fragments
  int initialized_checksum = 0;
  char dummy_data[dummydata_size];
  for (int idx = 0; idx < dummydata_size; ++idx) {
    int val = 0x7f & idx;
    dummy_data[idx] = val;
    initialized_checksum += val;
  }
  std::vector<StorageKey> key_list;
  for (int event_id = 1; event_id <= events_to_generate; ++event_id) {
    for (int geo_location = 0; geo_location < links_to_generate; ++geo_location) {
      StorageKey key(event_id, StorageKey::s_invalid_detector_id, geo_location);
      KeyedDataBlock data_block(key);
      data_block.m_unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.m_data_size = dummydata_size;
      data_store_ptr->write(data_block);
      key_list.push_back(key);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // create a new DataStore instance to read back the data that was written
  nlohmann::json read_conf ;
  read_conf["name"] = "tempReader" ;
  read_conf["filename_prefix"] = file_prefix ;
  read_conf["directory_path"] = file_path ;
  read_conf["mode"] = "one-fragment-per-file" ;
  std::unique_ptr<HDF5DataStore> data_store_ptr2(new HDF5DataStore(read_conf));

  // loop over all of the keys to read in the data
  for (size_t kdx = 0; kdx < key_list.size(); ++kdx) {
    KeyedDataBlock data_block = data_store_ptr2->read(key_list[kdx]);
    BOOST_REQUIRE_EQUAL(data_block.get_data_size_bytes(), dummydata_size);

    const char* data_ptr = static_cast<const char*>(data_block.get_data_start());
    int readback_checksum = 0;
    for (int idx = 0; idx < dummydata_size; ++idx) {
      readback_checksum += static_cast<int>(data_ptr[idx]);
    }
    BOOST_REQUIRE_EQUAL(readback_checksum, initialized_checksum);
  }
  data_store_ptr2.reset(); // explicit destruction

  // clean up the files that were created
  delete_files_matching_pattern(file_path, delete_pattern);
}

BOOST_AUTO_TEST_CASE(ReadEventFiles)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid());
  const int events_to_generate = 2;
  const int links_to_generate = 2;
  const int dummydata_size = 128;

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore instance for writing
  nlohmann::json conf ;
  conf["name"] = "tempWriter" ;
  conf["filename_prefix"] = file_prefix ;
  conf["directory_path"] = file_path ;
  conf["mode"] = "one-event-per-file" ;
  std::unique_ptr<HDF5DataStore> data_store_ptr(new HDF5DataStore( conf ));

  // write several events, each with several fragments
  int initialized_checksum = 0;
  char dummy_data[dummydata_size];
  for (int idx = 0; idx < dummydata_size; ++idx) {
    int val = 0x7f & idx;
    dummy_data[idx] = val;
    initialized_checksum += val;
  }
  std::vector<StorageKey> key_list;
  for (int event_id = 1; event_id <= events_to_generate; ++event_id) {
    for (int geo_location = 0; geo_location < links_to_generate; ++geo_location) {
      StorageKey key(event_id, StorageKey::s_invalid_detector_id, geo_location);
      KeyedDataBlock data_block(key);
      data_block.m_unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.m_data_size = dummydata_size;
      data_store_ptr->write(data_block);
      key_list.push_back(key);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // create a new DataStore instance to read back the data that was written
  nlohmann::json read_conf ;
  read_conf["name"] = "tempReader" ;
  read_conf["filename_prefix"] = file_prefix ;
  read_conf["directory_path"] = file_path ;
  read_conf["mode"] = "one-event-per-file" ;
  std::unique_ptr<HDF5DataStore> data_store_ptr2(new HDF5DataStore( read_conf  ));

  // loop over all of the keys to read in the data
  for (size_t kdx = 0; kdx < key_list.size(); ++kdx) {
    KeyedDataBlock data_block = data_store_ptr2->read(key_list[kdx]);
    BOOST_REQUIRE_EQUAL(data_block.get_data_size_bytes(), dummydata_size);

    const char* data_ptr = static_cast<const char*>(data_block.get_data_start());
    int readback_checksum = 0;
    for (int idx = 0; idx < dummydata_size; ++idx) {
      readback_checksum += static_cast<int>(data_ptr[idx]);
    }
    BOOST_REQUIRE_EQUAL(readback_checksum, initialized_checksum);
  }
  data_store_ptr2.reset(); // explicit destruction

  // clean up the files that were created
  delete_files_matching_pattern(file_path, delete_pattern);
}
*/
BOOST_AUTO_TEST_CASE(ReadSingleFile)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid());

  const int dummydata_size = 7;
  const int run_number = 52;
  const int trigger_count = 5;
  const std::string detector_name = "FELIX";
  const int apa_count = 3;
  const int link_count = 1;

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore instance for writing
  nlohmann::json conf;
  conf["name"] = "tempWriter";
  conf["directory_path"] = file_path;
  conf["mode"] = "all-per-file";
  nlohmann::json subconf;
  subconf["overall_prefix"] = file_prefix;
  conf["filename_parameters"] = subconf;
  std::unique_ptr<HDF5DataStore> data_store_ptr(new HDF5DataStore(conf));

  int initialized_checksum = 0;
  // write several events, each with several fragments
  char dummy_data[dummydata_size];
  for (int idx = 0; idx < dummydata_size; ++idx) {
    int val = 0x7f & idx;
    dummy_data[idx] = val;
    initialized_checksum += val;
  }
  std::vector<StorageKey> key_list;
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number) {
    for (int apa_number = 1; apa_number <= apa_count; ++apa_number) {
      for (int link_number = 1; link_number <= link_count; ++link_number) {
        StorageKey key(run_number, trigger_number, detector_name, apa_number, link_number);
        KeyedDataBlock data_block(key);
        data_block.m_unowned_data_start = static_cast<void*>(&dummy_data[0]);
        data_block.m_data_size = dummydata_size;
        data_store_ptr->write(data_block);
        key_list.push_back(key);
      }                   // link number
    }                     // apa number
  }                       // trigger number
  data_store_ptr.reset(); // explicit destruction

  // create a new DataStore instance to read back the data that was written
  nlohmann::json read_conf;
  read_conf["name"] = "tempReader";
  read_conf["directory_path"] = file_path;
  read_conf["mode"] = "all-per-file";
  nlohmann::json read_subconf;
  read_subconf["overall_prefix"] = file_prefix;
  read_conf["filename_parameters"] = read_subconf;
  std::unique_ptr<HDF5DataStore> data_store_ptr2(new HDF5DataStore(read_conf));

  // loop over all of the keys to read in the data
  for (size_t kdx = 0; kdx < key_list.size(); ++kdx) {
    KeyedDataBlock data_block = data_store_ptr2->read(key_list[kdx]);
    BOOST_REQUIRE_EQUAL(data_block.get_data_size_bytes(), dummydata_size);
    const char* data_ptr = static_cast<const char*>(data_block.get_data_start());
    int readback_checksum = 0;
    for (int idx = 0; idx < dummydata_size; ++idx) {
      readback_checksum += static_cast<int>(data_ptr[idx]);
    }
    BOOST_REQUIRE_EQUAL(readback_checksum, initialized_checksum);
  }
  data_store_ptr2.reset(); // explicit destruction

  // clean up the files that were created
  delete_files_matching_pattern(file_path, delete_pattern);
}

BOOST_AUTO_TEST_SUITE_END()
