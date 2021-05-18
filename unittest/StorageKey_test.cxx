/**
 * @file StorageKey_test.cxx StorageKey class Unit Tests
 *
 * The tests here primarily confirm that StorageKey stored key components
 * are what we expect them to be
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/StorageKey.hpp"

#define BOOST_TEST_MODULE StorageKey_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <string>

namespace {

constexpr int run_number = 1234;
constexpr int event_id = 111;
onstexpr int group_type = StorageKey::DataRecordGroupType::kTPC;
constexpr int region_number = 1;
constexpr int element_number = 1;
dunedaq::dfmodules::StorageKey stk(run_number, event_id, group_type, region_number, element_number); ///< StorageKey instance for the test

} // namespace ""

BOOST_AUTO_TEST_SUITE(StorageKey_test)

BOOST_AUTO_TEST_CASE(sanity_checks)
{
  int m_event_id = -999;
  std::string m_detector_id = "XXXXX";
  int m_geo_location = -999;
  BOOST_TEST_MESSAGE("Attempted StorageKey sanity checks for Key::event_id,detector_id,geo_location");
  m_event_id = stk.get_event_id();
  m_detector_id = stk.get_detector_id();
  m_geo_location = stk.get_geo_location();

  BOOST_CHECK_EQUAL(m_event_id, event_id);
  BOOST_CHECK_EQUAL(m_detector_id, detector_id);
  BOOST_CHECK_EQUAL(m_geo_location, geo_location);
}

using namespace dunedaq::dfmodules;

BOOST_AUTO_TEST_CASE(check_placeholder_values)
{
  const int sample_event_id = 1234;
  const std::string sample_detector_id = "TPC";
  const int sample_geo_location = 0;

  // Something would have to be very wrong for this test to fail...
  StorageKey key1(
    StorageKey::s_invalid_event_id, StorageKey::s_invalid_detector_id, StorageKey::s_invalid_geo_location);
  BOOST_CHECK_EQUAL(key1.get_event_id(), StorageKey::s_invalid_event_id);
  BOOST_CHECK_EQUAL(key1.get_detector_id(), StorageKey::s_invalid_detector_id);
  BOOST_CHECK_EQUAL(key1.get_geo_location(), StorageKey::s_invalid_geo_location);

  // check for some sort of weird cross-talk
  StorageKey key2(sample_event_id, StorageKey::s_invalid_detector_id, StorageKey::s_invalid_geo_location);
  BOOST_CHECK_EQUAL(key2.get_event_id(), sample_event_id);
  BOOST_CHECK_EQUAL(key2.get_detector_id(), StorageKey::s_invalid_detector_id);
  BOOST_CHECK_EQUAL(key2.get_geo_location(), StorageKey::s_invalid_geo_location);

  StorageKey key3(StorageKey::s_invalid_event_id, sample_detector_id, StorageKey::s_invalid_geo_location);
  BOOST_CHECK_EQUAL(key3.get_event_id(), StorageKey::s_invalid_event_id);
  BOOST_CHECK_EQUAL(key3.get_detector_id(), sample_detector_id);
  BOOST_CHECK_EQUAL(key3.get_geo_location(), StorageKey::s_invalid_geo_location);

  StorageKey key4(StorageKey::s_invalid_event_id, StorageKey::s_invalid_detector_id, sample_geo_location);
  BOOST_CHECK_EQUAL(key4.get_event_id(), StorageKey::s_invalid_event_id);
  BOOST_CHECK_EQUAL(key4.get_detector_id(), StorageKey::s_invalid_detector_id);
  BOOST_CHECK_EQUAL(key4.get_geo_location(), sample_geo_location);
}

BOOST_AUTO_TEST_SUITE_END()
