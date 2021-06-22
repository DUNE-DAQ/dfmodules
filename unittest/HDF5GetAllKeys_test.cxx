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

nlohmann::json
make_config(const std::string& instance_name,
            const std::string& path,
            const std::string& pattern,
            const std::string& mode)
{
  hdf5datastore::ConfParams params;
  params.name = instance_name;
  params.directory_path = path;
  params.filename_parameters.overall_prefix = pattern;
  params.mode = mode;

  nlohmann::json output;
  hdf5datastore::to_json(output, params);
  return output;
}

BOOST_AUTO_TEST_SUITE(HDF5GetAllKeys_test)

BOOST_AUTO_TEST_CASE(GetKeysFromFragmentFiles)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo1_" + std::to_string(getpid());
  const int events_to_generate = 5;
  const int links_to_generate = 3;
  const int run_number = 100;
  constexpr int dummydata_size = 20;

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore instance for writing
  auto conf = make_config("tempWriter", file_path, file_prefix, "one-fragment-per-file");
  std::unique_ptr<HDF5DataStore> data_store_ptr(new HDF5DataStore(conf));

  // write several events, each with several fragments
  std::array<char, dummydata_size> dummy_data;
  for (int trigger_number = 1; trigger_number <= events_to_generate; ++trigger_number) {
    for (int element_number = 0; element_number < links_to_generate; ++element_number) {
      StorageKey key(run_number,
                     trigger_number,
                     StorageKey::DataRecordGroupType::kTPC,
                     StorageKey::s_invalid_region_number,
                     element_number);
      KeyedDataBlock data_block(key);
      data_block.m_unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.m_data_size = dummydata_size;
      data_store_ptr->write(data_block);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // create a second DataStore instance to fetch the keys
  conf = make_config("hdfStore", file_path, file_prefix, "one-fragment-per-file");
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
    int trigger_number = key.get_trigger_number();
    int element_number = key.get_element_number();
    if (trigger_number > 0 && (static_cast<int>(trigger_number)) <= events_to_generate &&
        (static_cast<int>(element_number)) < links_to_generate) // element_number >= 0 &&
    {
      ++individual_key_count[trigger_number - 1][element_number]; // NOLINT
    } else {
      TLOG() << "Unexpected key found: trigger_number=" << trigger_number << ", element_number=" << element_number;
    }
  }
  int correctlyFoundKeyCount = 0;
  for (int edx = 0; edx < events_to_generate; ++edx) {
    for (int gdx = 0; gdx < links_to_generate; ++gdx) {
      if (individual_key_count[edx][gdx] == 1) {
        ++correctlyFoundKeyCount;
      } else {
        TLOG() << "Missing or duplicate key found:  trigger_number=" << (edx + 1) << ", element_number=" << gdx
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
  std::string file_prefix = "demo2_" + std::to_string(getpid());
  const int events_to_generate = 5;
  const int links_to_generate = 3;
  const int run_number = 100;
  constexpr int dummydata_size = 20;

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore instance for writing
  auto conf = make_config("tempWriter", file_path, file_prefix, "one-event-per-file");
  std::unique_ptr<HDF5DataStore> data_store_ptr(new HDF5DataStore(conf));

  // write several events, each with several fragments
  std::array<char, dummydata_size> dummy_data;
  for (int trigger_number = 1; trigger_number <= events_to_generate; ++trigger_number) {
    for (int element_number = 0; element_number < links_to_generate; ++element_number) {
      StorageKey key(run_number,
                     trigger_number,
                     StorageKey::DataRecordGroupType::kTPC,
                     StorageKey::s_invalid_region_number,
                     element_number);
      KeyedDataBlock data_block(key);
      data_block.m_unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.m_data_size = dummydata_size;
      data_store_ptr->write(data_block);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // create a second DataStore instance to fetch the keys
  conf = make_config("hdfStore", file_path, file_prefix, "one-event-per-file");
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
    int trigger_number = key.get_trigger_number();
    int element_number = key.get_element_number();
    if (trigger_number > 0 && (static_cast<int>(trigger_number)) <= events_to_generate &&
        (static_cast<int>(element_number)) < links_to_generate) // element_number >= 0 &&
    {
      ++individual_key_count[trigger_number - 1][element_number]; // NOLINT
    } else {
      TLOG() << "Unexpected key found: trigger_number=" << trigger_number << ", element_number=" << element_number;
    }
  }
  int correctlyFoundKeyCount = 0;
  for (int edx = 0; edx < events_to_generate; ++edx) {
    for (int gdx = 0; gdx < links_to_generate; ++gdx) {
      if (individual_key_count[edx][gdx] == 1) {
        ++correctlyFoundKeyCount;
      } else {
        TLOG() << "Missing or duplicate key found:  trigger_number=" << (edx + 1) << ", element_number=" << gdx
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
  std::string file_prefix = "demo3_" + std::to_string(getpid());
  const int events_to_generate = 5;
  const int links_to_generate = 3;
  const int run_number = 100;
  constexpr int dummydata_size = 20;

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore instance for writing
  auto conf = make_config("tempWriter", file_path, file_prefix, "all-per-file");
  std::unique_ptr<HDF5DataStore> data_store_ptr(new HDF5DataStore(conf));

  // write several events, each with several fragments
  std::array<char, dummydata_size> dummy_data;
  for (int trigger_number = 1; trigger_number <= events_to_generate; ++trigger_number) {
    for (int element_number = 0; element_number < links_to_generate; ++element_number) {
      StorageKey key(run_number,
                     trigger_number,
                     StorageKey::DataRecordGroupType::kTPC,
                     StorageKey::s_invalid_region_number,
                     element_number);
      KeyedDataBlock data_block(key);
      data_block.m_unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.m_data_size = dummydata_size;
      data_store_ptr->write(data_block);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // create a second DataStore instance to fetch the keys
  conf = make_config("tempWriter", file_path, file_prefix, "all-per-file");
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
    int trigger_number = key.get_trigger_number();
    int element_number = key.get_element_number();
    if (trigger_number > 0 && (static_cast<int>(trigger_number)) <= events_to_generate &&
        (static_cast<int>(element_number)) < links_to_generate) // element_number >= 0 &&
    {
      ++individual_key_count[trigger_number - 1][element_number]; // NOLINT
    } else {
      TLOG() << "Unexpected key found: trigger_number=" << trigger_number << ", element_number=" << element_number;
    }
  }
  int correctlyFoundKeyCount = 0;
  for (int edx = 0; edx < events_to_generate; ++edx) {
    for (int gdx = 0; gdx < links_to_generate; ++gdx) {
      if (individual_key_count[edx][gdx] == 1) {
        ++correctlyFoundKeyCount;
      } else {
        TLOG() << "Missing or duplicate key found:  trigger_number=" << (edx + 1) << ", element_number=" << gdx
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
  std::string file_prefix = "demo4_" + std::to_string(getpid());
  const int events_to_generate = 5;
  const int links_to_generate = 3;
  const int run_number = 100;
  constexpr int dummydata_size = 20;
  std::array<char, dummydata_size> dummy_data;
  std::unique_ptr<HDF5DataStore> data_store_ptr;
  std::vector<StorageKey> key_list;

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // ****************************************
  // * write some fragment-based-file data
  // ****************************************
  auto conf = make_config("hdfDataStore", file_path, file_prefix, "one-fragment-per-file");
  data_store_ptr.reset(new HDF5DataStore(conf));
  for (int trigger_number = 1; trigger_number <= events_to_generate; ++trigger_number) {
    for (int element_number = 0; element_number < links_to_generate; ++element_number) {
      StorageKey key(run_number,
                     trigger_number,
                     StorageKey::DataRecordGroupType::kTPC,
                     StorageKey::s_invalid_region_number,
                     element_number);
      KeyedDataBlock data_block(key);
      data_block.m_unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.m_data_size = dummydata_size;
      data_store_ptr->write(data_block);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // ****************************************
  // * write some event-based-file data
  // ****************************************
  conf = make_config("hdfDataStore", file_path, file_prefix, "one-event-per-file");
  data_store_ptr.reset(new HDF5DataStore(conf));
  for (int trigger_number = 1; trigger_number <= events_to_generate; ++trigger_number) {
    for (int element_number = 0; element_number < links_to_generate; ++element_number) {
      StorageKey key(run_number,
                     trigger_number,
                     StorageKey::DataRecordGroupType::kTPC,
                     StorageKey::s_invalid_region_number,
                     element_number);
      KeyedDataBlock data_block(key);
      data_block.m_unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.m_data_size = dummydata_size;
      data_store_ptr->write(data_block);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // ****************************************
  // * write some single-file data
  // ****************************************
  conf = make_config("hdfDataStore", file_path, file_prefix, "all-per-file");
  data_store_ptr.reset(new HDF5DataStore(conf));
  for (int trigger_number = 1; trigger_number <= events_to_generate; ++trigger_number) {
    for (int element_number = 0; element_number < links_to_generate; ++element_number) {
      StorageKey key(run_number,
                     trigger_number,
                     StorageKey::DataRecordGroupType::kTPC,
                     StorageKey::s_invalid_region_number,
                     element_number);
      KeyedDataBlock data_block(key);
      data_block.m_unowned_data_start = static_cast<void*>(&dummy_data[0]);
      data_block.m_data_size = dummydata_size;
      data_store_ptr->write(data_block);
    }
  }
  data_store_ptr.reset(); // explicit destruction

  // **************************************************
  // * check that fragment-based-file key lookup works
  // **************************************************
  conf = make_config("hdfDataStore", file_path, file_prefix, "one-fragment-per-file");
  data_store_ptr.reset(new HDF5DataStore(conf));
  key_list = data_store_ptr->get_all_existing_keys();
  BOOST_REQUIRE_EQUAL(key_list.size(), (events_to_generate * links_to_generate));

  // **************************************************
  // * check that event-based-file key lookup works
  // **************************************************
  conf = make_config("hdfDataStore", file_path, file_prefix, "one-event-per-file");
  data_store_ptr.reset(new HDF5DataStore(conf));
  key_list = data_store_ptr->get_all_existing_keys();
  BOOST_REQUIRE_EQUAL(key_list.size(), (events_to_generate * links_to_generate));

  // **************************************************
  // * check that single-file key lookup works
  // **************************************************
  conf = make_config("hdfDataStore", file_path, file_prefix, "all-per-file");
  data_store_ptr.reset(new HDF5DataStore(conf));
  key_list = data_store_ptr->get_all_existing_keys();
  BOOST_REQUIRE_EQUAL(key_list.size(), (events_to_generate * links_to_generate));

  // clean up the files that were created
  delete_files_matching_pattern(file_path, delete_pattern);
}

BOOST_AUTO_TEST_SUITE_END()
