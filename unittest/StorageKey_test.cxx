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

#include "ddpdemo/StorageKey.hpp"

#define BOOST_TEST_MODULE StorageKey_test // NOLINT

#include <boost/test/unit_test.hpp>

#include <string>

namespace {

constexpr int eventID = 111;
const std::string detectorID("LARTPC");
constexpr int geoLocation = 333;
dunedaq::ddpdemo::StorageKey stk(eventID, detectorID, geoLocation); ///< StorageKey instance for the test

} // namespace ""

BOOST_AUTO_TEST_SUITE(StorageKey_test)

BOOST_AUTO_TEST_CASE(sanity_checks)
{
  int m_eventID = -999;
  std::string m_detectorID = "XXXXX";
  int m_geoLocation = -999;
  BOOST_TEST_MESSAGE("Attempted StorageKey sanity checks for Key::eventID,detectorID,geoLocation");
  m_eventID = stk.getEventID();
  m_detectorID = stk.getDetectorID();
  m_geoLocation = stk.getGeoLocation();

  BOOST_CHECK_EQUAL(m_eventID, eventID);
  BOOST_CHECK_EQUAL(m_detectorID, detectorID);
  BOOST_CHECK_EQUAL(m_geoLocation, geoLocation);
}

using namespace dunedaq::ddpdemo;

BOOST_AUTO_TEST_CASE(check_placeholder_values)
{
  const int SAMPLE_EVENTID = 1234;
  const std::string SAMPLE_DETECTORID = "FELIX";
  const int SAMPLE_GEOLOCATION = 0;

  // Something would have to be very wrong for this test to fail...
  StorageKey key1(StorageKey::INVALID_EVENTID, StorageKey::INVALID_DETECTORID, StorageKey::INVALID_GEOLOCATION);
  BOOST_CHECK_EQUAL(key1.getEventID(), StorageKey::INVALID_EVENTID);
  BOOST_CHECK_EQUAL(key1.getDetectorID(), StorageKey::INVALID_DETECTORID);
  BOOST_CHECK_EQUAL(key1.getGeoLocation(), StorageKey::INVALID_GEOLOCATION);

  // check for some sort of weird cross-talk
  StorageKey key2(SAMPLE_EVENTID, StorageKey::INVALID_DETECTORID, StorageKey::INVALID_GEOLOCATION);
  BOOST_CHECK_EQUAL(key2.getEventID(), SAMPLE_EVENTID);
  BOOST_CHECK_EQUAL(key2.getDetectorID(), StorageKey::INVALID_DETECTORID);
  BOOST_CHECK_EQUAL(key2.getGeoLocation(), StorageKey::INVALID_GEOLOCATION);

  StorageKey key3(StorageKey::INVALID_EVENTID, SAMPLE_DETECTORID, StorageKey::INVALID_GEOLOCATION);
  BOOST_CHECK_EQUAL(key3.getEventID(), StorageKey::INVALID_EVENTID);
  BOOST_CHECK_EQUAL(key3.getDetectorID(), SAMPLE_DETECTORID);
  BOOST_CHECK_EQUAL(key3.getGeoLocation(), StorageKey::INVALID_GEOLOCATION);

  StorageKey key4(StorageKey::INVALID_EVENTID, StorageKey::INVALID_DETECTORID, SAMPLE_GEOLOCATION);
  BOOST_CHECK_EQUAL(key4.getEventID(), StorageKey::INVALID_EVENTID);
  BOOST_CHECK_EQUAL(key4.getDetectorID(), StorageKey::INVALID_DETECTORID);
  BOOST_CHECK_EQUAL(key4.getGeoLocation(), SAMPLE_GEOLOCATION);
}

BOOST_AUTO_TEST_SUITE_END()
