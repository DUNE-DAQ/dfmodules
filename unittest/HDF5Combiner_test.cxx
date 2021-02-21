/**
 * @file HDF5Combiner_test.cxx Application that tests and demonstrates
 * the combining of fragment files into event files and event files
 * into a single file.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "HDF5DataStore.hpp"

#define BOOST_TEST_MODULE HDF5Combiner_test // NOLINT

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

BOOST_AUTO_TEST_SUITE(HDF5Combiner_test)

BOOST_AUTO_TEST_CASE(CombineFragmentsIntoEvents)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid());
  const int events_to_generate = 5;
  const int links_to_generate = 10;
  const int dummydata_size = 4096;

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create an initial DataStore instance for writing the fragment files
  nlohmann::json conf;
  conf["name"] = "hdfDataStore";
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

  // check that the expected number of fragment-based files was created
  std::string search_pattern = file_prefix + "_event_\\d+_geoID_\\d+.hdf5";
  std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), (events_to_generate * links_to_generate));

  // now create two DataStore instances, one to read the fragment files and one to write event files
  conf["name"] = "hdfReader";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "one-fragment-per-file";
  std::unique_ptr<HDF5DataStore> input_ptr(new HDF5DataStore(conf));
  conf["name"] = "hdfWriter";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "one-event-per-file";
  std::unique_ptr<HDF5DataStore> output_ptr(new HDF5DataStore(conf));

  // fetch all of the keys that exist in the input DataStore
  std::vector<StorageKey> key_list = input_ptr->get_all_existing_keys();
  BOOST_REQUIRE_EQUAL(key_list.size(), (events_to_generate * links_to_generate));

  // copy each of the fragments (data blocks) from the input to the output
  for (auto& key : key_list) {
    KeyedDataBlock data_block = input_ptr->read(key);
    output_ptr->write(data_block);
  }
  input_ptr.reset();  // explicit destruction
  output_ptr.reset(); // explicit destruction

  // check that the expected number of event-based files was created
  search_pattern = file_prefix + "_event_\\d+.hdf5";
  file_list = get_files_matching_pattern(file_path, search_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), events_to_generate);

  // now create two DataStore instances, one to read the event files and one to write a single file
  conf["name"] = "hdfReader";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "one-event-per-file";
  input_ptr.reset(new HDF5DataStore(conf));
  conf["name"] = "hdfWriter";
  conf["filename_prefix"] = file_prefix;
  conf["directory_path"] = file_path;
  conf["mode"] = "all-per-file";
  output_ptr.reset(new HDF5DataStore(conf));

  // fetch all of the keys that exist in the input DataStore
  key_list = input_ptr->get_all_existing_keys();
  BOOST_REQUIRE_EQUAL(key_list.size(), (events_to_generate * links_to_generate));

  // copy each of the fragments (data blocks) from the input to the output
  for (auto& key : key_list) {
    KeyedDataBlock data_block = input_ptr->read(key);
    output_ptr->write(data_block);
  }
  input_ptr.reset();  // explicit destruction
  output_ptr.reset(); // explicit destruction

  // check that the expected single file was created
  search_pattern = file_prefix + "_all_events.hdf5";
  file_list = get_files_matching_pattern(file_path, search_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);

  // check that the size of the single file is reasonable
  //
  // (This check uses the BOOST_REQUIRE_CLOSE_FRACTION test to see if the size
  // of the fully-combined data file is "close enough" in size to the sum of all
  // the fragment data sizes.  Of course, the file size will be slightly larger than
  // the sum of the fragment sizes, because of the additional information that the
  // HDF5 format adds.  The HDF5 addition seems to be about 12%, so we allow for a 15%
  // difference, which is represented with the 0.15 in the third argument of the test call.)
  size_t single_file_size = std::filesystem::file_size(file_list[0]);
  BOOST_REQUIRE_CLOSE_FRACTION(static_cast<float>(single_file_size),
                               static_cast<float>(events_to_generate * links_to_generate * dummydata_size),
                               0.15);

  // clean up all of the files that were created
  file_list = delete_files_matching_pattern(file_path, delete_pattern);
}

BOOST_AUTO_TEST_SUITE_END()
