/**
 * @file HDF5KeyTranslator_test.cxx Test application that tests and demonstrates
 * the functionality of the HDF5KeyTranslator class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "../plugins/HDF5KeyTranslator.hpp"

#include "ers/ers.h"

#define BOOST_TEST_MODULE HDF5KeyTranslator_test // NOLINT

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

using namespace dunedaq::ddpdemo;

BOOST_AUTO_TEST_SUITE(HDF5KeyTranslator_test)

BOOST_AUTO_TEST_CASE(PathString)
{
  std::string path;

  StorageKey key1(1, "None", 2);
  path = HDF5KeyTranslator::getPathString(key1);
  BOOST_REQUIRE_EQUAL(path, "0001/002");

  StorageKey key2(12345, "None", 6);
  path = HDF5KeyTranslator::getPathString(key2);
  BOOST_REQUIRE_EQUAL(path, "12345/006");

  StorageKey key3(123, "None", 4567);
  path = HDF5KeyTranslator::getPathString(key3);
  BOOST_REQUIRE_EQUAL(path, "0123/4567");
}

BOOST_AUTO_TEST_CASE(PathElements)
{
  std::vector<std::string> elementList;

  StorageKey key1(1, "None", 2);
  elementList = HDF5KeyTranslator::getPathElements(key1);
  BOOST_REQUIRE_EQUAL(elementList.size(), 2);
  BOOST_REQUIRE_EQUAL(elementList[0], "0001");
  BOOST_REQUIRE_EQUAL(elementList[1], "002");
}

BOOST_AUTO_TEST_CASE(KeyFromString)
{
  StorageKey key(0, "", 0); // do we want to add support for a default constructor in StorageKey?

  std::string path1 = "1/2";
  key = HDF5KeyTranslator::getKeyFromString(path1, 1);
  BOOST_REQUIRE_EQUAL(key.getEventID(), 1);
  BOOST_REQUIRE_EQUAL(key.getDetectorID(), StorageKey::INVALID_DETECTORID);
  BOOST_REQUIRE_EQUAL(key.getGeoLocation(), 2);

  std::string path2 = "0003/004";
  key = HDF5KeyTranslator::getKeyFromString(path2, 1);
  BOOST_REQUIRE_EQUAL(key.getEventID(), 3);
  BOOST_REQUIRE_EQUAL(key.getDetectorID(), StorageKey::INVALID_DETECTORID);
  BOOST_REQUIRE_EQUAL(key.getGeoLocation(), 4);

  std::string path3 = "12345/6789";
  key = HDF5KeyTranslator::getKeyFromString(path3, 1);
  BOOST_REQUIRE_EQUAL(key.getEventID(), 12345);
  BOOST_REQUIRE_EQUAL(key.getDetectorID(), StorageKey::INVALID_DETECTORID);
  BOOST_REQUIRE_EQUAL(key.getGeoLocation(), 6789);
}

BOOST_AUTO_TEST_CASE(KeyFromList)
{
  StorageKey key(0, "", 0); // do we want to add support for a default constructor in StorageKey?

  std::vector<std::string> list1 = { "8", "9" };
  key = HDF5KeyTranslator::getKeyFromList(list1, 1);
  BOOST_REQUIRE_EQUAL(key.getEventID(), 8);
  BOOST_REQUIRE_EQUAL(key.getDetectorID(), StorageKey::INVALID_DETECTORID);
  BOOST_REQUIRE_EQUAL(key.getGeoLocation(), 9);
}

BOOST_AUTO_TEST_SUITE_END()
