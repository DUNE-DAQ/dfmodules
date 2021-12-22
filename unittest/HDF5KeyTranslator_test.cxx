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
  hdf5datastore::PathParams params1;
  params1.detector_group_type = "TPC";
  params1.detector_group_name = "TPC";
  params1.region_name_prefix = "APA";
  params1.digits_for_region_number = 3;
  params1.element_name_prefix = "Link";
  params1.digits_for_element_number = 2;

  hdf5datastore::PathParams params2;
  params2.detector_group_type = "PDS";
  params2.detector_group_name = "PDS";
  params2.region_name_prefix = "Region";
  params2.digits_for_region_number = 2;
  params2.element_name_prefix = "Element";
  params2.digits_for_element_number = 1;

  hdf5datastore::PathParamList param_list;
  param_list.push_back(params1);
  param_list.push_back(params2);

  hdf5datastore::FileLayoutParams layout_params;
  layout_params.trigger_record_name_prefix = "TrigRec";
  layout_params.digits_for_trigger_number = 4;
  layout_params.path_param_list = param_list;

  hdf5datastore::ConfParams config_params;
  config_params.file_layout_parameters = layout_params;

  HDF5KeyTranslator translator(config_params);
  std::string path;

  StorageKey key1(101, 1, StorageKey::DataRecordGroupType::kTPC, 2, 3);
  path = translator.get_path_string(key1);
  BOOST_REQUIRE_EQUAL(path, "TrigRec0001/TPC/APA002/Link03");

  StorageKey key2(101, 123456, StorageKey::DataRecordGroupType::kTPC, 6, 7);
  path = translator.get_path_string(key2);
  BOOST_REQUIRE_EQUAL(path, "TrigRec123456/TPC/APA006/Link07");

  StorageKey key3(101, 123, StorageKey::DataRecordGroupType::kPDS, 4567, 890);
  path = translator.get_path_string(key3);
  BOOST_REQUIRE_EQUAL(path, "TrigRec0123/PDS/Region4567/Element890");

  StorageKey key4(101, 22, StorageKey::DataRecordGroupType::kPDS, 3, 4);
  path = translator.get_path_string(key4);
  BOOST_REQUIRE_EQUAL(path, "TrigRec0022/PDS/Region03/Element4");
}

#if 0
BOOST_AUTO_TEST_CASE(PathElements)
{
  std::vector<std::string> element_list;

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
