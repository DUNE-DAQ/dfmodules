/**
 * @file HDF5KeyTranslator_test.cxx Test application that tests and demonstrates
 * the functionality of the HDF5KeyTranslator class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "HDF5KeyTranslator.hpp"
#include "dfmodules/hdf5datastore/Structs.hpp"

#define BOOST_TEST_MODULE HDF5KeyTranslator_test // NOLINT

#include "boost/test/unit_test.hpp"

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

  StorageKey key1(101, 1, StorageKey::DataRecordGroupType::kInvalid, 2, 3);
  path = HDF5KeyTranslator::get_path_string(key1, layout_params);
  BOOST_REQUIRE_EQUAL(path, "0001/Invalid/002/03");

  StorageKey key2(101, 12345, StorageKey::DataRecordGroupType::kInvalid, 6, 7);
  path = HDF5KeyTranslator::get_path_string(key2, layout_params);
  BOOST_REQUIRE_EQUAL(path, "12345/Invalid/006/07");

  StorageKey key3(101, 123, StorageKey::DataRecordGroupType::kInvalid, 4567, 890);
  path = HDF5KeyTranslator::get_path_string(key3, layout_params);
  BOOST_REQUIRE_EQUAL(path, "0123/Invalid/4567/890");

  layout_params.trigger_record_name_prefix = "TriggerRecord";
  layout_params.digits_for_trigger_number = 3;
  layout_params.apa_name_prefix = "APA";
  layout_params.digits_for_apa_number = 2;
  layout_params.link_name_prefix = "Link";
  layout_params.digits_for_link_number = 3;

  StorageKey key4(101, 22, StorageKey::DataRecordGroupType::kTPC, 33, 44);
  path = HDF5KeyTranslator::get_path_string(key4, layout_params);
  BOOST_REQUIRE_EQUAL(path, "TriggerRecord022/TPC/APA33/Link044");
}

#if 0
BOOST_AUTO_TEST_CASE(PathElements)
{
  std::vector<std::string> element_list;

  hdf5datastore::HDF5DataStoreFileLayoutParams layout_params;
  layout_params.trigger_record_name_prefix = "Test";
  layout_params.digits_for_trigger_number = 4;
  layout_params.apa_name_prefix = "Fake";
  layout_params.digits_for_apa_number = 3;
  layout_params.link_name_prefix = "Link";
  layout_params.digits_for_link_number = 2;

  StorageKey key1(101, 1, "Invalid", 2, 3); // run number, trigger number, detector name, APA number, link number
  element_list = HDF5KeyTranslator::get_path_elements(key1, layout_params);
  BOOST_REQUIRE_EQUAL(element_list.size(), 4);
  BOOST_REQUIRE_EQUAL(element_list[0], "Test0001");
  BOOST_REQUIRE_EQUAL(element_list[1], "Invalid");
  BOOST_REQUIRE_EQUAL(element_list[2], "Fake002");
  BOOST_REQUIRE_EQUAL(element_list[3], "Link03");
}
#endif

#if 0
BOOST_AUTO_TEST_CASE(KeyFromString)
{
  StorageKey key(0, "", 0); // do we want to add support for a default constructor in StorageKey?

  std::string path1 = "1/2";
  key = HDF5KeyTranslator::get_key_from_string(path1, 1);
  BOOST_REQUIRE_EQUAL(key.get_event_id(), 1);
  BOOST_REQUIRE_EQUAL(key.get_detector_id(), StorageKey::s_invalid_detector_id);
  BOOST_REQUIRE_EQUAL(key.get_geo_location(), 2);

  std::string path2 = "0003/004";
  key = HDF5KeyTranslator::get_key_from_string(path2, 1);
  BOOST_REQUIRE_EQUAL(key.get_event_id(), 3);
  BOOST_REQUIRE_EQUAL(key.get_detector_id(), StorageKey::s_invalid_detector_id);
  BOOST_REQUIRE_EQUAL(key.get_geo_location(), 4);

  std::string path3 = "12345/6789";
  key = HDF5KeyTranslator::get_key_from_string(path3, 1);
  BOOST_REQUIRE_EQUAL(key.get_event_id(), 12345);
  BOOST_REQUIRE_EQUAL(key.get_detector_id(), StorageKey::s_invalid_detector_id);
  BOOST_REQUIRE_EQUAL(key.get_geo_location(), 6789);
}

BOOST_AUTO_TEST_CASE(KeyFromList)
{
  StorageKey key(0, "", 0); // do we want to add support for a default constructor in StorageKey?

  std::vector<std::string> list1 = { "8", "9" };
  key = HDF5KeyTranslator::get_key_from_list(list1, 1);
  BOOST_REQUIRE_EQUAL(key.get_event_id(), 8);
  BOOST_REQUIRE_EQUAL(key.get_detector_id(), StorageKey::s_invalid_detector_id);
  BOOST_REQUIRE_EQUAL(key.get_geo_location(), 9);
}
#endif

BOOST_AUTO_TEST_SUITE_END()
