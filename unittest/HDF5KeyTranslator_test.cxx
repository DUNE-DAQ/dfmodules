/**
 * @file HDF5KeyTranslator_test.cxx Test application that tests and demonstrates
 * the functionality of the HDF5KeyTranslator class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "../plugins/HDF5KeyTranslator.hpp" // NOLINT
#include "../src/dfmodules/hdf5datastore/Structs.hpp" // NOLINT

#include "ers/ers.h"

#define BOOST_TEST_MODULE HDF5KeyTranslator_test // NOLINT

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

using namespace dunedaq::dfmodules;

BOOST_AUTO_TEST_SUITE(HDF5KeyTranslator_test)

BOOST_AUTO_TEST_CASE(PathString)
{
  std::string path;

  hdf5datastore::HDF5DataStoreFileLayoutParams layout_params;
  layout_params.trigger_record_name_prefix = "";
  layout_params.digits_for_trigger_number = 4;
  layout_params.apa_name_prefix = "";
  layout_params.digits_for_apa_number = 3;
  layout_params.link_name_prefix = "";
  layout_params.digits_for_link_number = 2;

  StorageKey key1(101, 1, "None", 2, 3);  // run number, trigger number, detector name, APA number, link number
  path = HDF5KeyTranslator::get_path_string(key1, layout_params);
  BOOST_REQUIRE_EQUAL(path, "0001/None/002/03");

  StorageKey key2(101, 12345, "None", 6, 7);
  path = HDF5KeyTranslator::get_path_string(key2, layout_params);
  BOOST_REQUIRE_EQUAL(path, "12345/None/006/07");

  StorageKey key3(101, 123, "None", 4567, 890);
  path = HDF5KeyTranslator::get_path_string(key3, layout_params);
  BOOST_REQUIRE_EQUAL(path, "0123/None/4567/890");

  layout_params.trigger_record_name_prefix = "TriggerRecord";
  layout_params.digits_for_trigger_number = 3;
  layout_params.apa_name_prefix = "APA";
  layout_params.digits_for_apa_number = 2;
  layout_params.link_name_prefix = "Link";
  layout_params.digits_for_link_number = 3;

  StorageKey key4(101, 22, "FELIX", 33, 44);
  path = HDF5KeyTranslator::get_path_string(key4, layout_params);
  BOOST_REQUIRE_EQUAL(path, "TriggerRecord022/FELIX/APA33/Link044");
}

BOOST_AUTO_TEST_CASE(PathElements)
{
  std::vector<std::string> elementList;

  hdf5datastore::HDF5DataStoreFileLayoutParams layout_params;
  layout_params.trigger_record_name_prefix = "Test";
  layout_params.digits_for_trigger_number = 4;
  layout_params.apa_name_prefix = "Fake";
  layout_params.digits_for_apa_number = 3;
  layout_params.link_name_prefix = "Link";
  layout_params.digits_for_link_number = 2;

  StorageKey key1(101, 1, "None", 2, 3);  // run number, trigger number, detector name, APA number, link number
  elementList = HDF5KeyTranslator::get_path_elements(key1, layout_params);
  BOOST_REQUIRE_EQUAL(elementList.size(), 4);
  BOOST_REQUIRE_EQUAL(elementList[0], "Test0001");
  BOOST_REQUIRE_EQUAL(elementList[1], "None");
  BOOST_REQUIRE_EQUAL(elementList[2], "Fake002");
  BOOST_REQUIRE_EQUAL(elementList[3], "Link03");
}

#if 0
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
#endif

BOOST_AUTO_TEST_SUITE_END()
