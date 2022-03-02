/**
 * @file DataFlowOrchestrator_test.cxx Test application that tests and demonstrates
 * the functionality of the DataFlowOrchestrator class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DataFlowOrchestrator.hpp"

#include "appfwk/DAQSink.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TriggerInhibit.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/datafloworchestratorinfo/InfoNljs.hpp"
#include "networkmanager/NetworkManager.hpp"

#define BOOST_TEST_MODULE DataFlowOrchestrator_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace dunedaq::dfmodules;

namespace dunedaq {

BOOST_AUTO_TEST_SUITE(DataFlowOrchestrator_test)

struct QueueRegistryFixture
{
  void setup()
  {
    std::map<std::string, appfwk::QueueConfig> queue_cfgs;
    const std::string queue_name = "trigger_decision_q";
    queue_cfgs[queue_name].kind = appfwk::QueueConfig::queue_kind::kFollySPSCQueue;
    queue_cfgs[queue_name].capacity = 1;
    appfwk::QueueRegistry::get().configure(queue_cfgs);
  }
};

BOOST_TEST_GLOBAL_FIXTURE(QueueRegistryFixture);

struct NetworkManagerTestFixture
{
  NetworkManagerTestFixture()
  {
    networkmanager::nwmgr::Connections testConfig;
    networkmanager::nwmgr::Connection testConn;
    testConn.name = "test.trigdec_0";
    testConn.address = "tcp://127.0.0.10:5050";
    testConfig.push_back(testConn);
    testConn.name = "test.trigdec";
    testConn.address = "inproc://foo";
    testConfig.push_back(testConn);
    testConn.name = "test.triginh";
    testConn.address = "inproc://bar";
    testConfig.push_back(testConn);
    testConn.name = "test.token";
    testConn.address = "inproc://baz";
    testConfig.push_back(testConn);
    networkmanager::NetworkManager::get().configure(testConfig);
  }

  ~NetworkManagerTestFixture() { networkmanager::NetworkManager::get().reset(); }

  NetworkManagerTestFixture(NetworkManagerTestFixture const&) = default;
  NetworkManagerTestFixture(NetworkManagerTestFixture&&) = default;
  NetworkManagerTestFixture& operator=(NetworkManagerTestFixture const&) = default;
  NetworkManagerTestFixture& operator=(NetworkManagerTestFixture&&) = default;
};

datafloworchestratorinfo::Info
get_dfo_info(std::shared_ptr<appfwk::DAQModule> dfo)
{
  opmonlib::InfoCollector ci;
  dfo->get_info(ci, 99);

  auto json = ci.get_collected_infos();
  auto info_json = json[opmonlib::JSONTags::properties][datafloworchestratorinfo::Info::info_type];
  datafloworchestratorinfo::Info info_obj;
  datafloworchestratorinfo::from_json(info_json[opmonlib::JSONTags::data], info_obj);

  return info_obj;
}
void
send_token(dfmessages::trigger_number_t trigger_number, std::string connection_name = "test.trigdec_0")
{
  dfmessages::TriggerDecisionToken token;
  token.run_number = 1;
  token.trigger_number = trigger_number;
  token.decision_destination = connection_name;
  auto tokenmsg = serialization::serialize(token, serialization::kMsgPack);
  TLOG() << "Sending TriggerDecisionToken with trigger number " << trigger_number << " to DFO";
  networkmanager::NetworkManager::get().send_to(
    "test.token", static_cast<const void*>(tokenmsg.data()), tokenmsg.size(), ipm::Sender::s_block);
}

void
recv_trigdec(ipm::Receiver::Response res)
{
  auto decision = serialization::deserialize<dfmessages::TriggerDecision>(res.data);
  TLOG() << "Received TriggerDecision with trigger number " << decision.trigger_number << " from DFO";
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  send_token(decision.trigger_number);
}

std::atomic<bool> busy_signal_recvd = false;
void
recv_triginh(ipm::Receiver::Response res)
{
  auto inhibit = serialization::deserialize<dfmessages::TriggerInhibit>(res.data);
  TLOG() << "Received TriggerInhibit with busy=" << std::boolalpha << inhibit.busy << " from DFO";
  busy_signal_recvd = inhibit.busy;
}

void
send_trigdec(dfmessages::trigger_number_t trigger_number)
{
  dunedaq::dfmessages::TriggerDecision td;
  td.trigger_number = trigger_number;
  td.run_number = 1;
  td.trigger_timestamp = 1;
  td.trigger_type = 1;
  td.readout_type = dunedaq::dfmessages::ReadoutType::kLocalized;
  TLOG() << "Sending TriggerDecision with trigger number " << trigger_number << " to DFO";
  auto decisionmsg = serialization::serialize(td, serialization::kMsgPack);
  networkmanager::NetworkManager::get().send_to(
    "test.trigdec", static_cast<const void*>(decisionmsg.data()), decisionmsg.size(), ipm::Sender::s_block);
}

BOOST_AUTO_TEST_CASE(CopyAndMoveSemantics)
{
  BOOST_REQUIRE(!std::is_copy_constructible_v<DataFlowOrchestrator>);
  BOOST_REQUIRE(!std::is_copy_assignable_v<DataFlowOrchestrator>);
  BOOST_REQUIRE(!std::is_move_constructible_v<DataFlowOrchestrator>);
  BOOST_REQUIRE(!std::is_move_assignable_v<DataFlowOrchestrator>);
}

BOOST_AUTO_TEST_CASE(Constructor)
{
  auto dfo = appfwk::make_module("DataFlowOrchestrator", "test");
}

BOOST_AUTO_TEST_CASE(Init)
{
  auto json = "{}"_json;
  auto dfo = appfwk::make_module("DataFlowOrchestrator", "test");
  dfo->init(json);
}

BOOST_FIXTURE_TEST_CASE(Commands, NetworkManagerTestFixture)
{
  auto json = "{}"_json;
  auto dfo = appfwk::make_module("DataFlowOrchestrator", "test");
  dfo->init(json);

  auto conf_json = "{\"dataflow_applications\": [ { \"thresholds\": { \"free\": 1, \"busy\": 2 }, "
                   "\"decision_connection\": \"test.trigdec_0\" } ], "
                   "\"general_queue_timeout\": 100, \"td_send_retries\": 5, \"token_connection\": \"test.token\", "
                   "\"busy_connection\": \"test.triginh\", \"td_connection\": \"test.trigdec\" }"_json;
  auto start_json = "{\"run\": 1}"_json;
  auto null_json = "{}"_json;

  dfo->execute_command("conf", conf_json);
  dfo->execute_command("start", start_json);
  dfo->execute_command("stop", null_json);
  dfo->execute_command("scrap", null_json);

  auto info = get_dfo_info(dfo);
  BOOST_REQUIRE_EQUAL(info.tokens_received, 0);
  BOOST_REQUIRE_EQUAL(info.decisions_received, 0);
  BOOST_REQUIRE_EQUAL(info.decisions_sent, 0);
  BOOST_REQUIRE_EQUAL(info.forwarding_decision, 0);
  BOOST_REQUIRE_EQUAL(info.waiting_for_decision, 0);
  BOOST_REQUIRE_EQUAL(info.deciding_destination, 0);
  BOOST_REQUIRE_EQUAL(info.waiting_for_token, 0);
  BOOST_REQUIRE_EQUAL(info.processing_token, 0);
}

BOOST_FIXTURE_TEST_CASE(DataFlow, NetworkManagerTestFixture)
{
  auto json = "{}"_json;
  auto dfo = appfwk::make_module("DataFlowOrchestrator", "test");
  dfo->init(json);

  auto conf_json = "{\"dataflow_applications\": [ { \"thresholds\": { \"free\": 1, \"busy\": 2 }, "
                   "\"decision_connection\": \"test.trigdec_0\" } ], "
                   "\"general_queue_timeout\": 100, \"td_send_retries\": 5, \"token_connection\": \"test.token\", "
                   "\"busy_connection\": \"test.triginh\", \"td_connection\": \"test.trigdec\" }"_json;
  auto start_json = "{\"run\": 1}"_json;
  auto null_json = "{}"_json;

  dfo->execute_command("conf", conf_json);

  networkmanager::NetworkManager::get().start_listening("test.trigdec_0");
  networkmanager::NetworkManager::get().register_callback("test.trigdec_0", recv_trigdec);

  networkmanager::NetworkManager::get().start_listening("test.triginh");
  networkmanager::NetworkManager::get().register_callback("test.triginh", recv_triginh);

  send_trigdec(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  send_token(999);
  send_token(9999);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  // Note: Counters are reset each time get_dfo_info is called!
  auto info = get_dfo_info(dfo);
  BOOST_REQUIRE_EQUAL(info.tokens_received, 0);

  dfo->execute_command("start", start_json);

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  info = get_dfo_info(dfo);
  BOOST_REQUIRE_EQUAL(info.tokens_received, 0);
  BOOST_REQUIRE_EQUAL(info.decisions_received, 0);
  BOOST_REQUIRE_EQUAL(info.decisions_sent, 0);

  send_trigdec(2);
  send_trigdec(3);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  send_trigdec(4);

  info = get_dfo_info(dfo);
  BOOST_REQUIRE_EQUAL(info.tokens_received, 0);
  BOOST_REQUIRE_EQUAL(info.decisions_received, 2);
  BOOST_REQUIRE_EQUAL(info.decisions_sent, 2);

  BOOST_REQUIRE(busy_signal_recvd.load());
  std::this_thread::sleep_for(std::chrono::milliseconds(400));

  info = get_dfo_info(dfo);
  BOOST_REQUIRE_EQUAL(info.tokens_received, 3);
  BOOST_REQUIRE_EQUAL(info.decisions_received, 1);
  BOOST_REQUIRE_EQUAL(info.decisions_sent, 1);
  BOOST_REQUIRE(!busy_signal_recvd.load());

  dfo->execute_command("stop", null_json);
  dfo->execute_command("scrap", null_json);
}

BOOST_FIXTURE_TEST_CASE(SendTrigDecFailed, NetworkManagerTestFixture)
{
  auto json = "{}"_json;
  auto dfo = appfwk::make_module("DataFlowOrchestrator", "test");
  dfo->init(json);

  auto conf_json = "{\"dataflow_applications\": [ { \"thresholds\": { \"free\": 1, \"busy\": 2 }, "
                   "\"decision_connection\": \"test.invalid_connection\" } ], "
                   "\"general_queue_timeout\": 100, \"td_send_retries\": 5, \"token_connection\": \"test.token\", "
                   "\"busy_connection\": \"test.triginh\", \"td_connection\": \"test.trigdec\" }"_json;
  auto start_json = "{\"run\": 1}"_json;
  auto null_json = "{}"_json;

  dfo->execute_command("conf", conf_json);

  dfo->execute_command("start", start_json);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  send_trigdec(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  auto info = get_dfo_info(dfo);
  BOOST_REQUIRE_EQUAL(info.tokens_received, 0);
  BOOST_REQUIRE_EQUAL(info.decisions_received, 1);
  BOOST_REQUIRE_EQUAL(info.decisions_sent, 0);

  // FWIW, tell the DFO to retry the invalid connection
  send_token(1000, "test.invalid_connection");
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Token for unknown dataflow app
  send_token(1000);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  dfo->execute_command("stop", null_json);
  dfo->execute_command("scrap", null_json);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace dunedaq
