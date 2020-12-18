/**
 * @file HDF5GetAllKeys_test.cxx Application that tests and demonstrates
 * the getAllExistingKeys() functionality of the HDF5DataStore class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "../plugins/HDF5DataStore.hpp"

#include "ers/ers.h"

#define BOOST_TEST_MODULE HDF5GetAllKeys_test // NOLINT

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

BOOST_AUTO_TEST_SUITE(HDF5GetAllKeys_test)

BOOST_AUTO_TEST_CASE(GetKeysFromFragmentFiles)
{
  std::string filePath(std::filesystem::temp_directory_path());
  std::string filePrefix = "demo" + std::to_string(getpid());
  const int EVENT_COUNT = 5;
  const int GEOLOC_COUNT = 3;
  const int DUMMYDATA_SIZE = 20;

  // delete any pre-existing files so that we start with a clean slate
  std::string deletePattern = filePrefix + ".*.hdf5";
  deleteFilesMatchingPattern(filePath, deletePattern);

  // create the DataStore instance for writing
  nlohmann::json conf ;
  conf["name"] = "tempWriter" ;
  conf["filename_prefix"] = filePrefix ; 
  conf["directory_path"] = filePath ; 
  conf["mode"] = "one-fragment-per-file" ;
  std::unique_ptr<HDF5DataStore> dsPtr( new HDF5DataStore(conf));

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

  // create a second DataStore instance to fetch the keys
  conf["name"] = "hdfStore" ;
  conf["filename_prefix"] = filePrefix ; 
  conf["directory_path"] = filePath ; 
  conf["mode"] = "one-fragment-per-file" ;
  dsPtr.reset(new HDF5DataStore( conf ));

  // fetch all of the keys that exist in the DataStore
  std::vector<StorageKey> keyList = dsPtr->getAllExistingKeys();
  BOOST_REQUIRE_EQUAL(keyList.size(), (EVENT_COUNT * GEOLOC_COUNT));

  // verify that all of the expected keys are present, there are no duplicates, etc.
  int individualKeyCount[EVENT_COUNT][GEOLOC_COUNT];
  for (int edx = 0; edx < EVENT_COUNT; ++edx) {
    for (int gdx = 0; gdx < GEOLOC_COUNT; ++gdx) {
      individualKeyCount[edx][gdx] = 0;
    }
  }
  for (auto& key : keyList) {
    int eventID = key.getEventID();
    int geoLoc = key.getGeoLocation();
    if (eventID > 0 && (static_cast<int>(eventID)) <= EVENT_COUNT &&
        (static_cast<int>(geoLoc)) < GEOLOC_COUNT) // geoLoc >= 0 &&
    {
      ++individualKeyCount[eventID - 1][geoLoc]; // NOLINT
    } else {
      ERS_LOG("Unexpected key found: eventID=" << eventID << ", geoLoc=" << geoLoc);
    }
  }
  int correctlyFoundKeyCount = 0;
  for (int edx = 0; edx < EVENT_COUNT; ++edx) {
    for (int gdx = 0; gdx < GEOLOC_COUNT; ++gdx) {
      if (individualKeyCount[edx][gdx] == 1) {
        ++correctlyFoundKeyCount;
      } else {
        ERS_LOG("Missing or duplicate key found:  eventID=" << (edx + 1) << ", geoLoc=" << gdx
                                                            << ", count=" << individualKeyCount[edx][gdx]);
      }
    }
  }
  BOOST_REQUIRE_EQUAL(correctlyFoundKeyCount, (EVENT_COUNT * GEOLOC_COUNT));
  dsPtr.reset(); // explicit destruction

  // clean up the files that were created
  deleteFilesMatchingPattern(filePath, deletePattern);
}

BOOST_AUTO_TEST_CASE(GetKeysFromEventFiles)
{
  std::string filePath(std::filesystem::temp_directory_path());
  std::string filePrefix = "demo" + std::to_string(getpid());
  const int EVENT_COUNT = 5;
  const int GEOLOC_COUNT = 3;
  const int DUMMYDATA_SIZE = 20;

  // delete any pre-existing files so that we start with a clean slate
  std::string deletePattern = filePrefix + ".*.hdf5";
  deleteFilesMatchingPattern(filePath, deletePattern);

  // create the DataStore instance for writing
  nlohmann::json conf ;
  conf["name"] = "tempWriter" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "one-event-per-file" ;
  std::unique_ptr<HDF5DataStore> dsPtr(new HDF5DataStore(conf));

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

  // create a second DataStore instance to fetch the keys
  conf["name"] = "hdfStore" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "one-event-per-file" ;
  dsPtr.reset(new HDF5DataStore(conf));

  // fetch all of the keys that exist in the DataStore
  std::vector<StorageKey> keyList = dsPtr->getAllExistingKeys();
  BOOST_REQUIRE_EQUAL(keyList.size(), (EVENT_COUNT * GEOLOC_COUNT));

  // verify that all of the expected keys are present, there are no duplicates, etc.
  int individualKeyCount[EVENT_COUNT][GEOLOC_COUNT];
  for (int edx = 0; edx < EVENT_COUNT; ++edx) {
    for (int gdx = 0; gdx < GEOLOC_COUNT; ++gdx) {
      individualKeyCount[edx][gdx] = 0;
    }
  }
  for (auto& key : keyList) {
    int eventID = key.getEventID();
    int geoLoc = key.getGeoLocation();
    if (eventID > 0 && (static_cast<int>(eventID)) <= EVENT_COUNT &&
        (static_cast<int>(geoLoc)) < GEOLOC_COUNT) // geoLoc >= 0 &&
    {
      ++individualKeyCount[eventID - 1][geoLoc]; // NOLINT
    } else {
      ERS_LOG("Unexpected key found: eventID=" << eventID << ", geoLoc=" << geoLoc);
    }
  }
  int correctlyFoundKeyCount = 0;
  for (int edx = 0; edx < EVENT_COUNT; ++edx) {
    for (int gdx = 0; gdx < GEOLOC_COUNT; ++gdx) {
      if (individualKeyCount[edx][gdx] == 1) {
        ++correctlyFoundKeyCount;
      } else {
        ERS_LOG("Missing or duplicate key found:  eventID=" << (edx + 1) << ", geoLoc=" << gdx
                                                            << ", count=" << individualKeyCount[edx][gdx]);
      }
    }
  }
  BOOST_REQUIRE_EQUAL(correctlyFoundKeyCount, (EVENT_COUNT * GEOLOC_COUNT));
  dsPtr.reset(); // explicit destruction

  // clean up the files that were created
  deleteFilesMatchingPattern(filePath, deletePattern);
}

BOOST_AUTO_TEST_CASE(GetKeysFromAllInOneFiles)
{
  std::string filePath(std::filesystem::temp_directory_path());
  std::string filePrefix = "demo" + std::to_string(getpid());
  const int EVENT_COUNT = 5;
  const int GEOLOC_COUNT = 3;
  const int DUMMYDATA_SIZE = 20;

  // delete any pre-existing files so that we start with a clean slate
  std::string deletePattern = filePrefix + ".*.hdf5";
  deleteFilesMatchingPattern(filePath, deletePattern);

  // create the DataStore instance for writing
  nlohmann::json conf ;
  conf["name"] = "tempWriter" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "all-per-file" ;
  std::unique_ptr<HDF5DataStore> dsPtr(new HDF5DataStore( conf ));

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

  // create a second DataStore instance to fetch the keys
  conf["name"] = "hdfStore" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "all-per-file" ;
  dsPtr.reset(new HDF5DataStore( conf ));

  // fetch all of the keys that exist in the DataStore
  std::vector<StorageKey> keyList = dsPtr->getAllExistingKeys();
  BOOST_REQUIRE_EQUAL(keyList.size(), (EVENT_COUNT * GEOLOC_COUNT));

  // verify that all of the expected keys are present, there are no duplicates, etc.
  int individualKeyCount[EVENT_COUNT][GEOLOC_COUNT];
  for (int edx = 0; edx < EVENT_COUNT; ++edx) {
    for (int gdx = 0; gdx < GEOLOC_COUNT; ++gdx) {
      individualKeyCount[edx][gdx] = 0;
    }
  }
  for (auto& key : keyList) {
    int eventID = key.getEventID();
    int geoLoc = key.getGeoLocation();
    if (eventID > 0 && (static_cast<int>(eventID)) <= EVENT_COUNT &&
        (static_cast<int>(geoLoc)) < GEOLOC_COUNT) // geoLoc >= 0 &&
    {
      ++individualKeyCount[eventID - 1][geoLoc]; // NOLINT
    } else {
      ERS_LOG("Unexpected key found: eventID=" << eventID << ", geoLoc=" << geoLoc);
    }
  }
  int correctlyFoundKeyCount = 0;
  for (int edx = 0; edx < EVENT_COUNT; ++edx) {
    for (int gdx = 0; gdx < GEOLOC_COUNT; ++gdx) {
      if (individualKeyCount[edx][gdx] == 1) {
        ++correctlyFoundKeyCount;
      } else {
        ERS_LOG("Missing or duplicate key found:  eventID=" << (edx + 1) << ", geoLoc=" << gdx
                                                            << ", count=" << individualKeyCount[edx][gdx]);
      }
    }
  }
  BOOST_REQUIRE_EQUAL(correctlyFoundKeyCount, (EVENT_COUNT * GEOLOC_COUNT));
  dsPtr.reset(); // explicit destruction

  // clean up the files that were created
  deleteFilesMatchingPattern(filePath, deletePattern);
}

BOOST_AUTO_TEST_CASE(CheckCrossTalk)
{
  std::string filePath(std::filesystem::temp_directory_path());
  std::string filePrefix = "demo" + std::to_string(getpid());
  const int EVENT_COUNT = 5;
  const int GEOLOC_COUNT = 3;
  const int DUMMYDATA_SIZE = 20;
  char dummyData[DUMMYDATA_SIZE];
  std::unique_ptr<HDF5DataStore> dsPtr;
  std::vector<StorageKey> keyList;

  // delete any pre-existing files so that we start with a clean slate
  std::string deletePattern = filePrefix + ".*.hdf5";
  deleteFilesMatchingPattern(filePath, deletePattern);

  // ****************************************
  // * write some fragment-based-file data
  // ****************************************
  nlohmann::json conf ;
  conf["name"] = "hdfDataStore" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "one-fragment-per-file" ;
  dsPtr.reset(new HDF5DataStore(conf));
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

  // ****************************************
  // * write some event-based-file data
  // ****************************************
  conf["name"] = "hdfDataStore" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "one-event-per-file" ;
  dsPtr.reset(new HDF5DataStore(conf));
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

  // ****************************************
  // * write some single-file data
  // ****************************************
  conf["name"] = "hdfDataStore" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "all-per-file" ;
  dsPtr.reset(new HDF5DataStore(conf));
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

  // **************************************************
  // * check that fragment-based-file key lookup works
  // **************************************************
  conf["name"] = "hdfDataStore" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "one-fragment-per-file" ;
  dsPtr.reset(new HDF5DataStore(conf));
  keyList = dsPtr->getAllExistingKeys();
  BOOST_REQUIRE_EQUAL(keyList.size(), (EVENT_COUNT * GEOLOC_COUNT));

  // **************************************************
  // * check that event-based-file key lookup works
  // **************************************************
  conf["name"] = "hdfDataStore" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "one-event-per-file" ;
  dsPtr.reset(new HDF5DataStore( conf ));
  keyList = dsPtr->getAllExistingKeys();
  BOOST_REQUIRE_EQUAL(keyList.size(), (EVENT_COUNT * GEOLOC_COUNT));

  // **************************************************
  // * check that single-file key lookup works
  // **************************************************
  conf["name"] = "hdfDataStore" ;
  conf["filename_prefix"] = filePrefix ;
  conf["directory_path"] = filePath ;
  conf["mode"] = "all-per-file" ;
  dsPtr.reset(new HDF5DataStore( conf ));
  keyList = dsPtr->getAllExistingKeys();
  BOOST_REQUIRE_EQUAL(keyList.size(), (EVENT_COUNT * GEOLOC_COUNT));

  // clean up the files that were created
  deleteFilesMatchingPattern(filePath, deletePattern);
}

BOOST_AUTO_TEST_SUITE_END()
