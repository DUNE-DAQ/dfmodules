/**
 * @file HDF5GetAllKeys_test.cxx Application that tests and demonstrates
 * the get_all_existing_keys() functionality of the HDF5DataStore class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "HDF5DataStore.hpp"

#include "logging/Logging.hpp"

#define BOOST_TEST_MODULE HDF5GetAllKeys_test // NOLINT

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

BOOST_AUTO_TEST_SUITE(HDF5GetAllKeys_test)

BOOST_AUTO_TEST_CASE(GetKeysFromFragmentFiles)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid());
  const int events_to_generate = 5;
  const int links_to_generate = 3;
  const int dummydata_size = 20;

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore instance for writing
  nlohmann::json conf;
  conf["name"] = "tempWriter";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "one-fragment-per-file";
  std::unique_ptr<HDF5DataStore> data_store_ptr(new HDF5DataStore(conf));

  // write several events, each with several fragments
  char dummy_data[dummydata_size];
  for (int event_id = 1; event_id <= events_to_generate; ++event_id) {
    for (int geo_location = 0; geo_location < links_to_generate; ++geo_location) {
      StorageKey key(event_id, StorageKey::s_invalid_detector_id, geo_location);
      KeyedDataBlock data_block(key);
      data_block.unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.data_size = dummydata_size;
      data_store_ptr->write(data_block);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // create a second DataStore instance to fetch the keys
  conf["name"] = "hdfStore";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "one-fragment-per-file";
  data_store_ptr.reset(new HDF5DataStore(conf));

  // fetch all of the keys that exist in the DataStore
  std::vector<StorageKey> key_list = data_store_ptr->get_all_existing_keys();
  BOOST_REQUIRE_EQUAL(key_list.size(), (events_to_generate * links_to_generate));

  // verify that all of the expected keys are present, there are no duplicates, etc.
  int individual_key_count[events_to_generate][links_to_generate];
  for (int edx = 0; edx < events_to_generate; ++edx) {
    for (int gdx = 0; gdx < links_to_generate; ++gdx) {
      individual_key_count[edx][gdx] = 0;
    }
  }
  for (auto& key : key_list) {
    int event_id = key.get_event_id();
    int geo_location = key.get_geo_location();
    if (event_id > 0 && (static_cast<int>(event_id)) <= events_to_generate &&
        (static_cast<int>(geo_location)) < links_to_generate) // geo_location >= 0 &&
    {
      ++individual_key_count[event_id - 1][geo_location]; // NOLINT
    } else {
		TLOG() << "Unexpected key found: event_id=" << event_id << ", geo_location=" << geo_location;
    }
  }
  int correctlyFoundKeyCount = 0;
  for (int edx = 0; edx < events_to_generate; ++edx) {
    for (int gdx = 0; gdx < links_to_generate; ++gdx) {
      if (individual_key_count[edx][gdx] == 1) {
        ++correctlyFoundKeyCount;
      } else {
        TLOG() << "Missing or duplicate key found:  event_id=" << (edx + 1) << ", geo_location=" << gdx
			   << ", count=" << individual_key_count[edx][gdx];
      }
    }
  }
  BOOST_REQUIRE_EQUAL(correctlyFoundKeyCount, (events_to_generate * links_to_generate));
  data_store_ptr.reset(); // explicit destruction

  // clean up the files that were created
  delete_files_matching_pattern(file_path, delete_pattern);
}

BOOST_AUTO_TEST_CASE(GetKeysFromEventFiles)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid());
  const int events_to_generate = 5;
  const int links_to_generate = 3;
  const int dummydata_size = 20;

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore instance for writing
  nlohmann::json conf;
  conf["name"] = "tempWriter";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "one-event-per-file";
  std::unique_ptr<HDF5DataStore> data_store_ptr(new HDF5DataStore(conf));

  // write several events, each with several fragments
  char dummy_data[dummydata_size];
  for (int event_id = 1; event_id <= events_to_generate; ++event_id) {
    for (int geo_location = 0; geo_location < links_to_generate; ++geo_location) {
      StorageKey key(event_id, StorageKey::s_invalid_detector_id, geo_location);
      KeyedDataBlock data_block(key);
      data_block.unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.data_size = dummydata_size;
      data_store_ptr->write(data_block);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // create a second DataStore instance to fetch the keys
  conf["name"] = "hdfStore";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "one-event-per-file";
  data_store_ptr.reset(new HDF5DataStore(conf));

  // fetch all of the keys that exist in the DataStore
  std::vector<StorageKey> key_list = data_store_ptr->get_all_existing_keys();
  BOOST_REQUIRE_EQUAL(key_list.size(), (events_to_generate * links_to_generate));

  // verify that all of the expected keys are present, there are no duplicates, etc.
  int individual_key_count[events_to_generate][links_to_generate];
  for (int edx = 0; edx < events_to_generate; ++edx) {
    for (int gdx = 0; gdx < links_to_generate; ++gdx) {
      individual_key_count[edx][gdx] = 0;
    }
  }
  for (auto& key : key_list) {
    int event_id = key.get_event_id();
    int geo_location = key.get_geo_location();
    if (event_id > 0 && (static_cast<int>(event_id)) <= events_to_generate &&
        (static_cast<int>(geo_location)) < links_to_generate) // geo_location >= 0 &&
    {
      ++individual_key_count[event_id - 1][geo_location]; // NOLINT
    } else {
      TLOG() << "Unexpected key found: event_id=" << event_id << ", geo_location=" << geo_location;
    }
  }
  int correctlyFoundKeyCount = 0;
  for (int edx = 0; edx < events_to_generate; ++edx) {
    for (int gdx = 0; gdx < links_to_generate; ++gdx) {
      if (individual_key_count[edx][gdx] == 1) {
        ++correctlyFoundKeyCount;
      } else {
        TLOG() << "Missing or duplicate key found:  event_id=" << (edx + 1) << ", geo_location=" << gdx
			   << ", count=" << individual_key_count[edx][gdx];
      }
    }
  }
  BOOST_REQUIRE_EQUAL(correctlyFoundKeyCount, (events_to_generate * links_to_generate));
  data_store_ptr.reset(); // explicit destruction

  // clean up the files that were created
  delete_files_matching_pattern(file_path, delete_pattern);
}

BOOST_AUTO_TEST_CASE(GetKeysFromAllInOneFiles)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid());
  const int events_to_generate = 5;
  const int links_to_generate = 3;
  const int dummydata_size = 20;

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore instance for writing
  nlohmann::json conf;
  conf["name"] = "tempWriter";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "all-per-file";
  std::unique_ptr<HDF5DataStore> data_store_ptr(new HDF5DataStore(conf));

  // write several events, each with several fragments
  char dummy_data[dummydata_size];
  for (int event_id = 1; event_id <= events_to_generate; ++event_id) {
    for (int geo_location = 0; geo_location < links_to_generate; ++geo_location) {
      StorageKey key(event_id, StorageKey::s_invalid_detector_id, geo_location);
      KeyedDataBlock data_block(key);
      data_block.unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.data_size = dummydata_size;
      data_store_ptr->write(data_block);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // create a second DataStore instance to fetch the keys
  conf["name"] = "hdfStore";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "all-per-file";
  data_store_ptr.reset(new HDF5DataStore(conf));

  // fetch all of the keys that exist in the DataStore
  std::vector<StorageKey> key_list = data_store_ptr->get_all_existing_keys();
  BOOST_REQUIRE_EQUAL(key_list.size(), (events_to_generate * links_to_generate));

  // verify that all of the expected keys are present, there are no duplicates, etc.
  int individual_key_count[events_to_generate][links_to_generate];
  for (int edx = 0; edx < events_to_generate; ++edx) {
    for (int gdx = 0; gdx < links_to_generate; ++gdx) {
      individual_key_count[edx][gdx] = 0;
    }
  }
  for (auto& key : key_list) {
    int event_id = key.get_event_id();
    int geo_location = key.get_geo_location();
    if (event_id > 0 && (static_cast<int>(event_id)) <= events_to_generate &&
        (static_cast<int>(geo_location)) < links_to_generate) // geo_location >= 0 &&
    {
      ++individual_key_count[event_id - 1][geo_location]; // NOLINT
    } else {
      TLOG() << "Unexpected key found: event_id=" << event_id << ", geo_location=" << geo_location;
    }
  }
  int correctlyFoundKeyCount = 0;
  for (int edx = 0; edx < events_to_generate; ++edx) {
    for (int gdx = 0; gdx < links_to_generate; ++gdx) {
      if (individual_key_count[edx][gdx] == 1) {
        ++correctlyFoundKeyCount;
      } else {
        TLOG() << "Missing or duplicate key found:  event_id=" << (edx + 1) << ", geo_location=" << gdx
			   << ", count=" << individual_key_count[edx][gdx];
      }
    }
  }
  BOOST_REQUIRE_EQUAL(correctlyFoundKeyCount, (events_to_generate * links_to_generate));
  data_store_ptr.reset(); // explicit destruction

  // clean up the files that were created
  delete_files_matching_pattern(file_path, delete_pattern);
}

BOOST_AUTO_TEST_CASE(CheckCrossTalk)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid());
  const int events_to_generate = 5;
  const int links_to_generate = 3;
  const int dummydata_size = 20;
  char dummy_data[dummydata_size];
  std::unique_ptr<HDF5DataStore> data_store_ptr;
  std::vector<StorageKey> key_list;

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // ****************************************
  // * write some fragment-based-file data
  // ****************************************
  nlohmann::json conf;
  conf["name"] = "hdfDataStore";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "one-fragment-per-file";
  data_store_ptr.reset(new HDF5DataStore(conf));
  for (int event_id = 1; event_id <= events_to_generate; ++event_id) {
    for (int geo_location = 0; geo_location < links_to_generate; ++geo_location) {
      StorageKey key(event_id, StorageKey::s_invalid_detector_id, geo_location);
      KeyedDataBlock data_block(key);
      data_block.unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.data_size = dummydata_size;
      data_store_ptr->write(data_block);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // ****************************************
  // * write some event-based-file data
  // ****************************************
  conf["name"] = "hdfDataStore";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "one-event-per-file";
  data_store_ptr.reset(new HDF5DataStore(conf));
  for (int event_id = 1; event_id <= events_to_generate; ++event_id) {
    for (int geo_location = 0; geo_location < links_to_generate; ++geo_location) {
      StorageKey key(event_id, StorageKey::s_invalid_detector_id, geo_location);
      KeyedDataBlock data_block(key);
      data_block.unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.data_size = dummydata_size;
      data_store_ptr->write(data_block);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // ****************************************
  // * write some single-file data
  // ****************************************
  conf["name"] = "hdfDataStore";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "all-per-file";
  data_store_ptr.reset(new HDF5DataStore(conf));
  for (int event_id = 1; event_id <= events_to_generate; ++event_id) {
    for (int geo_location = 0; geo_location < links_to_generate; ++geo_location) {
      StorageKey key(event_id, StorageKey::s_invalid_detector_id, geo_location);
      KeyedDataBlock data_block(key);
      data_block.unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.data_size = dummydata_size;
      data_store_ptr->write(data_block);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // **************************************************
  // * check that fragment-based-file key lookup works
  // **************************************************
  conf["name"] = "hdfDataStore";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "one-fragment-per-file";
  data_store_ptr.reset(new HDF5DataStore(conf));
  key_list = data_store_ptr->get_all_existing_keys();
  BOOST_REQUIRE_EQUAL(key_list.size(), (events_to_generate * links_to_generate));

  // **************************************************
  // * check that event-based-file key lookup works
  // **************************************************
  conf["name"] = "hdfDataStore";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "one-event-per-file";
  data_store_ptr.reset(new HDF5DataStore(conf));
  key_list = data_store_ptr->get_all_existing_keys();
  BOOST_REQUIRE_EQUAL(key_list.size(), (events_to_generate * links_to_generate));

  // **************************************************
  // * check that single-file key lookup works
  // **************************************************
  conf["name"] = "hdfDataStore";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "all-per-file";
  data_store_ptr.reset(new HDF5DataStore(conf));
  key_list = data_store_ptr->get_all_existing_keys();
  BOOST_REQUIRE_EQUAL(key_list.size(), (events_to_generate * links_to_generate));

  // clean up the files that were created
  delete_files_matching_pattern(file_path, delete_pattern);
}

BOOST_AUTO_TEST_SUITE_END()
