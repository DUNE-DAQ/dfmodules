/**
 * @file HDF5Combiner_test.cxx Application that tests and demonstrates
 * the combining of fragment files into event files and event files
 * into a single file.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "../plugins/HDF5DataStore.hpp"

#include "ers/ers.h"

#define BOOST_TEST_MODULE HDF5Combiner_test // NOLINT

#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <vector>

using namespace dunedaq::ddpdemo;

std::vector<std::string>
getFilesMatchingPattern(const std::string& path, const std::string& pattern)
{
  std::regex regexSearchPattern(pattern);
  std::vector<std::string> fileList;
  for (const auto& entry : std::filesystem::directory_iterator(path)) {
    if (std::regex_match(entry.path().filename().string(), regexSearchPattern)) {
      fileList.push_back(entry.path());
    }
  }
  return fileList;
}

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

BOOST_AUTO_TEST_SUITE(HDF5Combiner_test)

BOOST_AUTO_TEST_CASE(CombineFragmentsIntoEvents)
{
  std::string filePath(std::filesystem::temp_directory_path());
  std::string filePrefix = "demo" + std::to_string(getpid());
  const int EVENT_COUNT = 5;
  const int GEOLOC_COUNT = 10;
  const int DUMMYDATA_SIZE = 4096;

  // delete any pre-existing files so that we start with a clean slate
  std::string deletePattern = filePrefix + ".*.hdf5";
  deleteFilesMatchingPattern(filePath, deletePattern);

  // create an initial DataStore instance for writing the fragment files
  nlohmann::json conf ;
  conf["name"] = "hdfDataStore" ;
  conf["filename_prefix"] = filePrefix ; 
  conf["directory_path"] = filePath ; 
  conf["mode"] = "one-fragment-per-file" ;
  std::unique_ptr<HDF5DataStore> dsPtr( new HDF5DataStore(conf) );

  // write several events, each with several fragments
  char dummyData[DUMMYDATA_SIZE];
  for (int eventID = 1; eventID <= EVENT_COUNT; ++eventID) {
    for (int geoLoc = 0; geoLoc < GEOLOC_COUNT; ++geoLoc) {
      StorageKey key(eventID, StorageKey::INVALID_DETECTORID, geoLoc);
      KeyedDataBlock dataBlock(key);
      dataBlock.unowned_data_start = static_cast<void*>(&dummyData[0]);
      dataBlock.data_size = DUMMYDATA_SIZE;
      dsPtr->write(dataBlock);
    }
  }
  dsPtr.reset(); // explicit destruction

  // check that the expected number of fragment-based files was created
  std::string searchPattern = filePrefix + "_event_\\d+_geoID_\\d+.hdf5";
  std::vector<std::string> fileList = getFilesMatchingPattern(filePath, searchPattern);
  BOOST_REQUIRE_EQUAL(fileList.size(), (EVENT_COUNT * GEOLOC_COUNT));

  // now create two DataStore instances, one to read the fragment files and one to write event files
  conf["name"] = "hdfReader" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "one-fragment-per-file" ;
  std::unique_ptr<HDF5DataStore> inputPtr( new HDF5DataStore(conf) );
  conf["name"] = "hdfWriter" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "one-event-per-file" ;
  std::unique_ptr<HDF5DataStore> outputPtr(new HDF5DataStore( conf ));

  // fetch all of the keys that exist in the input DataStore
  std::vector<StorageKey> keyList = inputPtr->getAllExistingKeys();
  BOOST_REQUIRE_EQUAL(keyList.size(), (EVENT_COUNT * GEOLOC_COUNT));

  // copy each of the fragments (data blocks) from the input to the output
  for (auto& key : keyList) {
    KeyedDataBlock dataBlock = inputPtr->read(key);
    outputPtr->write(dataBlock);
  }
  inputPtr.reset();  // explicit destruction
  outputPtr.reset(); // explicit destruction

  // check that the expected number of event-based files was created
  searchPattern = filePrefix + "_event_\\d+.hdf5";
  fileList = getFilesMatchingPattern(filePath, searchPattern);
  BOOST_REQUIRE_EQUAL(fileList.size(), EVENT_COUNT);

  // now create two DataStore instances, one to read the event files and one to write a single file
  conf["name"] = "hdfReader" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "one-event-per-file" ;
  inputPtr.reset(new HDF5DataStore( conf ) );
  conf["name"] = "hdfWriter" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "all-per-file" ;
  outputPtr.reset(new HDF5DataStore( conf ) ) ;

  // fetch all of the keys that exist in the input DataStore
  keyList = inputPtr->getAllExistingKeys();
  BOOST_REQUIRE_EQUAL(keyList.size(), (EVENT_COUNT * GEOLOC_COUNT));

  // copy each of the fragments (data blocks) from the input to the output
  for (auto& key : keyList) {
    KeyedDataBlock dataBlock = inputPtr->read(key);
    outputPtr->write(dataBlock);
  }
  inputPtr.reset();  // explicit destruction
  outputPtr.reset(); // explicit destruction

  // check that the expected single file was created
  searchPattern = filePrefix + "_all_events.hdf5";
  fileList = getFilesMatchingPattern(filePath, searchPattern);
  BOOST_REQUIRE_EQUAL(fileList.size(), 1);

  // check that the size of the single file is reasonable
  //
  // (This check uses the BOOST_REQUIRE_CLOSE_FRACTION test to see if the size
  // of the fully-combined data file is "close enough" in size to the sum of all
  // the fragment data sizes.  Of course, the file size will be slightly larger than
  // the sum of the fragment sizes, because of the additional information that the
  // HDF5 format adds.  The HDF5 addition seems to be about 12%, so we allow for a 15%
  // difference, which is represented with the 0.15 in the third argument of the test call.)
  size_t singleFileSize = std::filesystem::file_size(fileList[0]);
  BOOST_REQUIRE_CLOSE_FRACTION(
    static_cast<float>(singleFileSize), static_cast<float>(EVENT_COUNT * GEOLOC_COUNT * DUMMYDATA_SIZE), 0.15);

  // clean up all of the files that were created
  fileList = deleteFilesMatchingPattern(filePath, deletePattern);
}

BOOST_AUTO_TEST_SUITE_END()
