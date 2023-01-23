/**
 * @file DataFlowOrchestrator_test.cxx Test application that tests and demonstrates
 * the functionality of the DataFlowOrchestrator class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DataFlowOrchestrator.hpp"

#include "appfwk/app/Nljs.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TriggerInhibit.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/datafloworchestratorinfo/InfoNljs.hpp"
#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"

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
    setenv("DUNEDAQ_PARTITION", "DataFlowOrchestrator_t", 0);

    dunedaq::iomanager::Queues_t queues;
    queues.emplace_back(dunedaq::iomanager::QueueConfig{
      { "trigger_decision_q", "TriggerDecision" }, dunedaq::iomanager::QueueType::kFollySPSCQueue, 1 });
    dunedaq::iomanager::Connections_t connections;
    connections.emplace_back(dunedaq::iomanager::Connection{
      { "test.trigdec_0", "TriggerDecision" }, "tcp://127.0.0.10:5050", dunedaq::iomanager::ConnectionType::kSendRecv });
    connections.emplace_back(dunedaq::iomanager::Connection{
      { "test.trigdec", "TriggerDecision" }, "inproc://trigdec", dunedaq::iomanager::ConnectionType::kSendRecv });
    connections.emplace_back(dunedaq::iomanager::Connection{
      { "test.triginh", "TriggerInhibit" }, "inproc://triginh", dunedaq::iomanager::ConnectionType::kSendRecv });
    connections.emplace_back(dunedaq::iomanager::Connection{
      { "test.token", "TriggerDecisionToken" }, "inproc://token", dunedaq::iomanager::ConnectionType::kSendRecv });

    dunedaq::get_iomanager()->configure(queues, connections, false, 0ms); // Not using ConfigClient
  }
  ~ConfigurationTestFixture() { dunedaq::get_iomanager()->reset(); }

  ConfigurationTestFixture(ConfigurationTestFixture const&) = default;
  ConfigurationTestFixture(ConfigurationTestFixture&&) = default;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture const&) = default;
  ConfigurationTestFixture& operator=(ConfigurationTestFixture&&) = default;

  static nlohmann::json make_init_json()
  {
    dunedaq::appfwk::app::ModInit data;
    data.conn_refs.emplace_back(dunedaq::appfwk::app::ConnectionReference{ "td_connection", "test.trigdec" });
    data.conn_refs.emplace_back(dunedaq::appfwk::app::ConnectionReference{ "token_connection", "test.token" });
    data.conn_refs.emplace_back(dunedaq::appfwk::app::ConnectionReference{ "busy_connection", "test.triginh" });
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
send_init_token(std::string connection_name = "test.trigdec_0")
{
  dfmessages::TriggerDecisionToken token;
  token.run_number = 0;
  token.trigger_number = 0;
  token.decision_destination = connection_name;

  TLOG() << "Sending Init TriggerDecisionToken to DFO";
  get_iom_sender<dfmessages::TriggerDecisionToken>("test.token")->send(std::move(token), iomanager::Sender::s_block);
}
void
send_token(dfmessages::trigger_number_t trigger_number,
           std::string connection_name = "test.trigdec_0",
           bool different_run = false)
{
  dfmessages::TriggerDecisionToken token;
  token.run_number = different_run ? 2 : 1;
  token.trigger_number = trigger_number;
  token.decision_destination = connection_name;

  TLOG() << "Sending TriggerDecisionToken with trigger number " << trigger_number << " to DFO";
  get_iom_sender<dfmessages::TriggerDecisionToken>("test.token")->send(std::move(token), iomanager::Sender::s_block);
}

void
recv_trigdec(const dfmessages::TriggerDecision& decision)
{
  TLOG() << "Received TriggerDecision with trigger number " << decision.trigger_number << " from DFO";
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  send_token(decision.trigger_number);
}

std::atomic<bool> busy_signal_recvd = false;
void
recv_triginh(const dfmessages::TriggerInhibit& inhibit)
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
  iom->get_sender<dfmessages::TriggerDecision>("test.trigdec")->send(std::move(td), iomanager::Sender::s_block);
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

  auto conf_json = "{\"thresholds\": { \"free\": 1, \"busy\": 2 }, "
                   "\"general_queue_timeout\": 100, \"td_send_retries\": 5}"_json;
  auto start_json = "{\"run\": 1}"_json;
  auto null_json = "{}"_json;

  dfo->execute_command("conf", "INITIAL", conf_json);
  dfo->execute_command("start", "CONFIGURED", start_json);
  dfo->execute_command("drain_dataflow", "RUNNING", null_json);
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

  auto conf_json = "{\"thresholds\": { \"free\": 1, \"busy\": 2 }, "
                   "\"general_queue_timeout\": 100, \"td_send_retries\": 5}"_json;
  auto start_json = "{\"run\": 1}"_json;
  auto null_json = "{}"_json;

  dfo->execute_command("conf", "INITIAL", conf_json);

  auto iom = iomanager::IOManager::get();
  iom->get_receiver<dfmessages::TriggerDecision>("test.trigdec_0")->add_callback(recv_trigdec);
  iom->get_receiver<dfmessages::TriggerInhibit>("test.triginh")->add_callback(recv_triginh);

  send_trigdec(1, true);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  send_token(999, "test.trigdec_0", true);
  send_token(9999, "test.trigdec_0", true);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  // Note: Counters are reset each time get_dfo_info is called!
  auto info = get_dfo_info(dfo);
  BOOST_REQUIRE_EQUAL(info.tokens_received, 0);

  dfo->execute_command("start", "CONFIGURED", start_json);
  send_init_token();

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

  dfo->execute_command("drain_dataflow", "RUNNING", null_json);
  dfo->execute_command("scrap", "CONFIGURED", null_json);
}

BOOST_FIXTURE_TEST_CASE(SendTrigDecFailed, ConfigurationTestFixture)
{
  auto json = ConfigurationTestFixture::make_init_json();
  auto dfo = appfwk::make_module("DataFlowOrchestrator", "test");
  dfo->init(json);

  auto conf_json = "{\"thresholds\": { \"free\": 1, \"busy\": 2 }, "
                   "\"general_queue_timeout\": 100, \"td_send_retries\": 5}"_json;
  auto start_json = "{\"run\": 1}"_json;
  auto null_json = "{}"_json;

  dfo->execute_command("conf", "INITIAL", conf_json);

  dfo->execute_command("start", "CONFIGURED", start_json);

  send_init_token("test.invalid_connection");

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

  dfo->execute_command("drain_dataflow", "RUNNING", null_json);
  dfo->execute_command("scrap", "CONFIGURED", null_json);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace dunedaq
