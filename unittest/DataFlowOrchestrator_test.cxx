/**
 * @file DataFlowOrchestrator_test.cxx Test application that tests and demonstrates
 * the functionality of the DataFlowOrchestrator class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DataFlowOrchestrator.hpp"
#include "dfmodules/hdf5datastore/Structs.hpp"

#define BOOST_TEST_MODULE DataFlowOrchestrator_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <string>
#include <vector>

using namespace dunedaq::dfmodules;

BOOST_AUTO_TEST_SUITE(DataFlowOrchestrator_test)

BOOST_AUTO_TEST_CASE(CopyAndMoveSemantics)
{
  BOOST_REQUIRE(!std::is_copy_constructible_v<DataFlowOrchestrator>);
  BOOST_REQUIRE(!std::is_copy_assignable_v<DataFlowOrchestrator>);
  BOOST_REQUIRE(!std::is_move_constructible_v<DataFlowOrchestrator>);
  BOOST_REQUIRE(!std::is_move_assignable_v<DataFlowOrchestrator>);
}

BOOST_AUTO_TEST_CASE(Constructor)
{
  auto dfo = dunedaq::appfwk::make_module("DataFlowOrchestrator", "test");
}

BOOST_AUTO_TEST_CASE(Init)
{
  std::map<std::string, dunedaq::appfwk::QueueConfig> queue_cfgs;
  const std::string queue_name = "trigger_decision_q";
  queue_cfgs[queue_name].kind = dunedaq::appfwk::QueueConfig::queue_kind::kFollySPSCQueue;
  queue_cfgs[queue_name].capacity = 2;
  dunedaq::appfwk::QueueRegistry::get().configure(queue_cfgs);
  auto json =
    "{\"qinfos\": [{ \"dir\": \"input\", \"inst\": \"trigger_decision_q\", \"name\": \"trigger_decision_queue\" }]}"_json;
  auto dfo = dunedaq::appfwk::make_module("DataFlowOrchestrator", "test");
  dfo->init(json);
}

BOOST_AUTO_TEST_SUITE_END()
