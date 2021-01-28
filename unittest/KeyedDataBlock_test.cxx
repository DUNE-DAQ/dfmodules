/**
 * @file KeyedDataBlock_test.cxx Test application that tests and demonstrates
 * the functionality of the KeyedDataBlock class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/KeyedDataBlock.hpp"

#include "ers/ers.h"

#define BOOST_TEST_MODULE KeyedDataBlock_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <memory>
#include <utility>

using namespace dunedaq::dfmodules;

BOOST_AUTO_TEST_SUITE(KeyedDataBlock_test)

BOOST_AUTO_TEST_CASE(SimpleLocalBuffer)
{
  const int buffer_size = 100;
  StorageKey sample_key(1, "2", 3);
  void* buff_ptr = malloc(buffer_size);
  memset(buff_ptr, 'X', buffer_size);

  KeyedDataBlock data_block(sample_key);
  data_block.unowned_data_start = buff_ptr;
  data_block.data_size = buffer_size;

  // this test simply checks that we get back the same pointer that
  // we passed into the KeyedDataBlock instance.
  BOOST_REQUIRE_EQUAL(data_block.get_data_start(), buff_ptr);

  // also the buffer size
  BOOST_REQUIRE_EQUAL(data_block.get_data_size_bytes(), buffer_size);

  // also the key
  BOOST_REQUIRE_EQUAL(data_block.data_key.get_event_id(), sample_key.get_event_id());
  BOOST_REQUIRE_EQUAL(data_block.data_key.get_geo_location(), sample_key.get_geo_location());

  free(buff_ptr);
}

BOOST_AUTO_TEST_CASE(LocalBufferOwnership)
{
  const int buffer_size = 100;
  StorageKey sample_key(1, "2", 3);
  void* buff_ptr = malloc(buffer_size);
  memset(buff_ptr, 'X', buffer_size);

  {
    // the main purpose of this block is to construct a KeyedDataBlock object
    // and then have it go out of scope so that it gets destructed.
    KeyedDataBlock data_block(sample_key);
    data_block.unowned_data_start = buff_ptr;
    data_block.data_size = buffer_size;

    BOOST_REQUIRE_EQUAL(data_block.get_data_start(), buff_ptr);
    BOOST_REQUIRE_EQUAL(data_block.get_data_size_bytes(), buffer_size);
    BOOST_REQUIRE_EQUAL(data_block.data_key.get_event_id(), sample_key.get_event_id());
    BOOST_REQUIRE_EQUAL(data_block.data_key.get_geo_location(), sample_key.get_geo_location());
  }

  // the following statements should *not* cause the test program to crash
  // (if everything is working well), because the KeyedDataBlock destruction
  // at the end of the code block above should *not* have deleted the memory
  // that we malloc-ed at the start of this test.
  BOOST_REQUIRE_EQUAL(static_cast<char*>(buff_ptr)[0], 'X');
  free(buff_ptr);
}

BOOST_AUTO_TEST_CASE(TransferredBufferOwnership)
{
  const int buffer_size = 100;
  StorageKey sample_key(1, "2", 3);
  char* buff_ptr = new char[buffer_size];
  memset(buff_ptr, 'X', buffer_size);
  std::unique_ptr<char> buffer_ptr(buff_ptr);
  BOOST_REQUIRE(buff_ptr[0] == 'X');

  {
    // the main purpose of this block is to construct a KeyedDataBlock object
    // and then have it go out of scope so that it gets destructed.
    KeyedDataBlock data_block(sample_key);
    data_block.owned_data_start = std::move(buffer_ptr);
    data_block.data_size = buffer_size;

    // check that the bare pointer that we get back is what we expect
    BOOST_REQUIRE_EQUAL(data_block.get_data_start(), buff_ptr);
  }

  // The goal of the following statement is to confirm that the memory created above
  // has been correctly deleted when the KeyedDataBlock instance in the code block
  // above goes out of scope and is destructed.
  // This test is currently working, but I wonder if that is just good luck.
  // It is probably 'undefined behavior' to access deleted memory like this, so
  // maybe we should also add a signal hander to avoid crashes...
  BOOST_REQUIRE_NE(buff_ptr[0], 'X');
}

BOOST_AUTO_TEST_SUITE_END()
