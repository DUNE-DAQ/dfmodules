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
constexpr int trigger_number = 111;
constexpr dunedaq::dfmodules::StorageKey::DataRecordGroupType group_type =
  dunedaq::dfmodules::StorageKey::DataRecordGroupType::kTPC;
constexpr int region_number = 1;
constexpr int element_number = 1;
dunedaq::dfmodules::StorageKey stk(run_number,
                                   trigger_number,
                                   group_type,
                                   region_number,
                                   element_number); ///< StorageKey instance for the test

} // namespace ""

BOOST_AUTO_TEST_SUITE(StorageKey_test)

BOOST_AUTO_TEST_CASE(sanity_checks)
{
  int check_run_number = -99;
  int check_trigger_number = -999;
  dunedaq::dfmodules::StorageKey::DataRecordGroupType check_group_type =
    dunedaq::dfmodules::StorageKey::DataRecordGroupType::kInvalid;
  int check_region_number = -9999;
  int check_element_number = -99999;
  BOOST_TEST_MESSAGE(
    "Attempted StorageKey sanity checks for Key::run_number,trigger_number,detector_type,region_number,element_number");
  check_run_number = stk.get_run_number();
  check_trigger_number = stk.get_trigger_number();
  check_group_type = stk.get_group_type();
  check_region_number = stk.get_region_number();
  check_element_number = stk.get_element_number();

  BOOST_CHECK_EQUAL(check_run_number, run_number);
  BOOST_CHECK_EQUAL(check_trigger_number, trigger_number);
  BOOST_CHECK_EQUAL(check_group_type, group_type);
  BOOST_CHECK_EQUAL(check_region_number, region_number);
  BOOST_CHECK_EQUAL(check_element_number, element_number);
}

using namespace dunedaq::dfmodules;

BOOST_AUTO_TEST_CASE(check_placeholder_values)
{
  const int sample_run_number = 0x5678;
  const int sample_trigger_number = 0x1234;
  const int sample_region_number = 0x9ABC;
  const int sample_element_number = 0;
  constexpr StorageKey::DataRecordGroupType sample_group_type = StorageKey::DataRecordGroupType::kTPC;

  // Something would have to be very wrong for this test to fail...
  StorageKey key1(StorageKey::s_invalid_run_number,
                  StorageKey::s_invalid_trigger_number,
                  StorageKey::DataRecordGroupType::kInvalid,
                  StorageKey::s_invalid_region_number,
                  StorageKey::s_invalid_element_number);
  BOOST_CHECK_EQUAL(key1.get_run_number(), StorageKey::s_invalid_run_number);
  BOOST_CHECK_EQUAL(key1.get_trigger_number(), StorageKey::s_invalid_trigger_number);
  BOOST_CHECK_EQUAL(key1.get_group_type(), StorageKey::DataRecordGroupType::kInvalid);
  BOOST_CHECK_EQUAL(key1.get_region_number(), StorageKey::s_invalid_region_number);
  BOOST_CHECK_EQUAL(key1.get_element_number(), StorageKey::s_invalid_element_number);

  // check for some sort of weird cross-talk
  StorageKey key2(sample_run_number,
                  StorageKey::s_invalid_trigger_number,
                  StorageKey::DataRecordGroupType::kInvalid,
                  StorageKey::s_invalid_region_number,
                  StorageKey::s_invalid_element_number);
  BOOST_CHECK_EQUAL(key2.get_run_number(), sample_run_number);
  BOOST_CHECK_EQUAL(key2.get_trigger_number(), StorageKey::s_invalid_trigger_number);
  BOOST_CHECK_EQUAL(key2.get_group_type(), StorageKey::DataRecordGroupType::kInvalid);
  BOOST_CHECK_EQUAL(key2.get_region_number(), StorageKey::s_invalid_region_number);
  BOOST_CHECK_EQUAL(key2.get_element_number(), StorageKey::s_invalid_element_number);

  StorageKey key3(sample_run_number,
                  sample_trigger_number,
                  StorageKey::DataRecordGroupType::kInvalid,
                  StorageKey::s_invalid_region_number,
                  StorageKey::s_invalid_element_number);
  BOOST_CHECK_EQUAL(key3.get_run_number(), sample_run_number);
  BOOST_CHECK_EQUAL(key3.get_trigger_number(), sample_trigger_number);
  BOOST_CHECK_EQUAL(key3.get_group_type(), StorageKey::DataRecordGroupType::kInvalid);
  BOOST_CHECK_EQUAL(key3.get_region_number(), StorageKey::s_invalid_region_number);
  BOOST_CHECK_EQUAL(key3.get_element_number(), StorageKey::s_invalid_element_number);

  StorageKey key4(sample_run_number,
                  sample_trigger_number,
                  sample_group_type,
                  StorageKey::s_invalid_region_number,
                  StorageKey::s_invalid_element_number);
  BOOST_CHECK_EQUAL(key4.get_run_number(), sample_run_number);
  BOOST_CHECK_EQUAL(key4.get_trigger_number(), sample_trigger_number);
  BOOST_CHECK_EQUAL(key4.get_group_type(), sample_group_type);
  BOOST_CHECK_EQUAL(key4.get_region_number(), StorageKey::s_invalid_region_number);
  BOOST_CHECK_EQUAL(key4.get_element_number(), StorageKey::s_invalid_element_number);

  StorageKey key5(sample_run_number,
                  sample_trigger_number,
                  sample_group_type,
                  sample_region_number,
                  StorageKey::s_invalid_element_number);
  BOOST_CHECK_EQUAL(key5.get_run_number(), sample_run_number);
  BOOST_CHECK_EQUAL(key5.get_trigger_number(), sample_trigger_number);
  BOOST_CHECK_EQUAL(key5.get_group_type(), sample_group_type);
  BOOST_CHECK_EQUAL(key5.get_region_number(), sample_region_number);
  BOOST_CHECK_EQUAL(key5.get_element_number(), StorageKey::s_invalid_element_number);

  StorageKey key6(
    sample_run_number, sample_trigger_number, sample_group_type, sample_region_number, sample_element_number);
  BOOST_CHECK_EQUAL(key6.get_run_number(), sample_run_number);
  BOOST_CHECK_EQUAL(key6.get_trigger_number(), sample_trigger_number);
  BOOST_CHECK_EQUAL(key6.get_group_type(), sample_group_type);
  BOOST_CHECK_EQUAL(key6.get_region_number(), sample_region_number);
  BOOST_CHECK_EQUAL(key6.get_element_number(), sample_element_number);
}

BOOST_AUTO_TEST_SUITE_END()
