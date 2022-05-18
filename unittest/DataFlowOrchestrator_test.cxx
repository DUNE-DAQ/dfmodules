/**
 * @file DataFlowOrchestrator_test.cxx Test application that tests and demonstrates
 * the functionality of the DataFlowOrchestrator class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DataFlowOrchestrator.hpp"

#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TriggerInhibit.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/datafloworchestratorinfo/InfoNljs.hpp"
#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"
#include "appfwk/app/Nljs.hpp"

#define BOOST_TEST_MODULE DataFlowOrchestrator_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace dunedaq::dfmodules;

namespace dunedaq {

BOOST_AUTO_TEST_SUITE(DataFlowOrchestrator_test)

struct ConfigurationTestFixture
{
    ConfigurationTestFixture()
    {
        dunedaq::iomanager::ConnectionIds_t connections;
        connections.emplace_back(
            dunedaq::iomanager::ConnectionId{ "test.trigdec_0_s", dunedaq::iomanager::ServiceType::kNetSender, "dfmessages::TriggerDecision", "tcp://127.0.0.10:5050" });
        connections.emplace_back(
            dunedaq::iomanager::ConnectionId{ "test.trigdec_s", dunedaq::iomanager::ServiceType::kNetSender, "dfmessages::TriggerDecision", "inproc://trigdec" });
        connections.emplace_back(
            dunedaq::iomanager::ConnectionId{ "test.triginh_s", dunedaq::iomanager::ServiceType::kNetSender, "dfmessages::TriggerInhibit", "inproc://triginh" });
        connections.emplace_back(
            dunedaq::iomanager::ConnectionId{ "test.token_s", dunedaq::iomanager::ServiceType::kNetSender, "dfmessages::TriggerDecisionToken", "inproc://token" });
        connections.emplace_back(
            dunedaq::iomanager::ConnectionId{ "test.trigdec_r", dunedaq::iomanager::ServiceType::kNetReceiver, "dfmessages::TriggerDecision", "inproc://trigdec" });
        connections.emplace_back(
            dunedaq::iomanager::ConnectionId{ "test.triginh_r", dunedaq::iomanager::ServiceType::kNetReceiver, "dfmessages::TriggerInhibit", "inproc://triginh" });
        connections.emplace_back(
            dunedaq::iomanager::ConnectionId{ "test.token_r", dunedaq::iomanager::ServiceType::kNetReceiver, "dfmessages::TriggerDecisionToken", "inproc://token" });
        connections.emplace_back(
            dunedaq::iomanager::ConnectionId{ "test.trigdec_0_r", dunedaq::iomanager::ServiceType::kNetReceiver, "dfmessages::TriggerDecision", "tcp://127.0.0.10:5050" });
        connections.emplace_back(
            dunedaq::iomanager::ConnectionId{ "trigger_decision_q", dunedaq::iomanager::ServiceType::kQueue, "dfmessages::TriggerDecision", "queue://FollySPSCQueue:1" });
        
        dunedaq::get_iomanager()->configure(connections);
    }
    ~ConfigurationTestFixture() { dunedaq::get_iomanager()->reset(); }

    ConfigurationTestFixture(ConfigurationTestFixture const&) = default;
    ConfigurationTestFixture(ConfigurationTestFixture&&) = default;
    ConfigurationTestFixture& operator=(ConfigurationTestFixture const&) = default;
    ConfigurationTestFixture& operator=(ConfigurationTestFixture&&) = default;

    static nlohmann::json make_init_json() {
        dunedaq::appfwk::app::ModInit data;
        data.conn_refs.emplace_back(
            dunedaq::iomanager::ConnectionRef{ "td_connection", "test.trigdec_r" }
        );
        data.conn_refs.emplace_back(
            dunedaq::iomanager::ConnectionRef{ "token_connection", "test.token_r" }
        );
        data.conn_refs.emplace_back(
            dunedaq::iomanager::ConnectionRef{ "busy_connection", "test.triginh_s" }
        );
        nlohmann::json json;
        dunedaq::appfwk::app::to_json(json, data);
        return json;
    }
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
send_token(dfmessages::trigger_number_t trigger_number, std::string connection_name = "test.trigdec_0_s", bool different_run = false)
{
  dfmessages::TriggerDecisionToken token;
  token.run_number = different_run ?  2 : 1;
  token.trigger_number = trigger_number;
  token.decision_destination = connection_name;

  TLOG() << "Sending TriggerDecisionToken with trigger number " << trigger_number << " to DFO";
  get_iom_sender<dfmessages::TriggerDecisionToken>(  "test.token_s" ) -> send( std::move(token), iomanager::Sender::s_block );
}

void
recv_trigdec(const dfmessages::TriggerDecision & decision)
{
  TLOG() << "Received TriggerDecision with trigger number " << decision.trigger_number << " from DFO";
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  send_token(decision.trigger_number);
}

std::atomic<bool> busy_signal_recvd = false;
void
recv_triginh(const dfmessages::TriggerInhibit & inhibit)
{
  TLOG() << "Received TriggerInhibit with busy=" << std::boolalpha << inhibit.busy << " from DFO";
  busy_signal_recvd = inhibit.busy;
}

void
send_trigdec(dfmessages::trigger_number_t trigger_number, bool different_run = false)
{
  dunedaq::dfmessages::TriggerDecision td;
  td.trigger_number = trigger_number;
  td.run_number = different_run ? 2 : 1;
  td.trigger_timestamp = 1;
  td.trigger_type = 1;
  td.readout_type = dunedaq::dfmessages::ReadoutType::kLocalized;
  auto iom = iomanager::IOManager::get();
  TLOG() << "Sending TriggerDecision with trigger number " << trigger_number << " to DFO";
  iom->get_sender<dfmessages::TriggerDecision>( "test.trigdec_s") -> send( std::move(td), iomanager::Sender::s_block );
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

BOOST_FIXTURE_TEST_CASE(Init, ConfigurationTestFixture)
{
  auto json = ConfigurationTestFixture::make_init_json();
  auto dfo = appfwk::make_module("DataFlowOrchestrator", "test");
  dfo->init(json);
}

BOOST_FIXTURE_TEST_CASE(Commands, ConfigurationTestFixture)
{
    auto json = ConfigurationTestFixture::make_init_json();
  auto dfo = appfwk::make_module("DataFlowOrchestrator", "test");
  dfo->init(json);

  auto conf_json = "{\"dataflow_applications\": [ { \"thresholds\": { \"free\": 1, \"busy\": 2 }, "
                   "\"connection_uid\": \"test.trigdec_0\" } ], "
                   "\"general_queue_timeout\": 100, \"td_send_retries\": 5 }"_json;
  auto start_json = "{\"run\": 1}"_json;
  auto null_json = "{}"_json;

  dfo->execute_command("conf", "INITIAL", conf_json);
  dfo->execute_command("start", "CONFIGURED", start_json);
  dfo->execute_command("stop", "RUNNING", null_json);
  dfo->execute_command("scrap", "CONFIGURED", null_json);

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

BOOST_FIXTURE_TEST_CASE(DataFlow, ConfigurationTestFixture)
{
    auto json = ConfigurationTestFixture::make_init_json();
  auto dfo = appfwk::make_module("DataFlowOrchestrator", "test");
  dfo->init(json);

  auto conf_json = "{\"dataflow_applications\": [ { \"thresholds\": { \"free\": 1, \"busy\": 2 }, "
                   "\"connection_uid\": \"test.trigdec_0_s\" } ], "
                   "\"general_queue_timeout\": 100, \"td_send_retries\": 5}"_json;
  auto start_json = "{\"run\": 1}"_json;
  auto null_json = "{}"_json;

  dfo->execute_command("conf", "INITIAL", conf_json);

  auto iom = iomanager::IOManager::get();
  iom->get_receiver<dfmessages::TriggerDecision>( "test.trigdec_0_r") -> add_callback(recv_trigdec);
  iom->get_receiver<dfmessages::TriggerInhibit>( "test.triginh_r") -> add_callback(recv_triginh );
  
  send_trigdec(1, true);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  
  send_token(999, "test.trigdec_0_s", true);
  send_token(9999, "test.trigdec_0_s", true);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  // Note: Counters are reset each time get_dfo_info is called!
  auto info = get_dfo_info(dfo);
  BOOST_REQUIRE_EQUAL(info.tokens_received, 0);
  
  dfo->execute_command("start", "CONFIGURED", start_json);
  
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

  dfo->execute_command("stop", "RUNNING", null_json);
  dfo->execute_command("scrap", "CONFIGURED", null_json);
}

BOOST_FIXTURE_TEST_CASE(SendTrigDecFailed, ConfigurationTestFixture)
{
    auto json = ConfigurationTestFixture::make_init_json();
  auto dfo = appfwk::make_module("DataFlowOrchestrator", "test");
  dfo->init(json);

  auto conf_json = "{\"dataflow_applications\": [ { \"thresholds\": { \"free\": 1, \"busy\": 2 }, "
                   "\"connection_uid\": \"test.invalid_connection\" } ], "
                   "\"general_queue_timeout\": 100, \"td_send_retries\": 5 }"_json;
  auto start_json = "{\"run\": 1}"_json;
  auto null_json = "{}"_json;

  dfo->execute_command("conf", "INITIAL", conf_json);

  dfo->execute_command("start", "CONFIGURED", start_json);

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

  dfo->execute_command("stop", "RUNNING", null_json);
  dfo->execute_command("scrap", "CONFIGURED", null_json);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace dunedaq
