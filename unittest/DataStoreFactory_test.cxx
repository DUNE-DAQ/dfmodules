/**
 * @file DataStoreFactory_test.cxx Test application that tests and demonstrates
 * the functionality of the mechanisms to allocate DataStores via the factory.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "ddpdemo/DataStore.hpp"

#include "ers/ers.h"

#define BOOST_TEST_MODULE DataStoreFactory_test // NOLINT

#include <boost/test/unit_test.hpp>

#include <memory>

using namespace dunedaq::ddpdemo;

BOOST_AUTO_TEST_SUITE(DataStoreFactory_test)



BOOST_AUTO_TEST_CASE(invalid_request)
{

  // we want to pass an invalid DataStore type and see if we get an exception
  BOOST_CHECK_THROW( makeDataStore( "dummy", nlohmann::json{} ), std::exception ) ;

  // we want to ask for a DataStore configuring absolutely nothing, not even its type
  // and check if we get an exception
  BOOST_CHECK_THROW( makeDataStore( nlohmann::json{} ), std::exception ) ;

}

/*
BOOST_AUTO_TEST_CASE(valid_request)
{

  nlohmann::json conf ;
  conf["name"] = "test" ;

  // we want to ask for a valid Data Store type
  auto ds = makeDataStore( "TrashCanDataStore", conf ) ;

  // and we want to check if we created something valid
  BOOST_TEST( ds.get() ) ;

}
*/

BOOST_AUTO_TEST_SUITE_END()
