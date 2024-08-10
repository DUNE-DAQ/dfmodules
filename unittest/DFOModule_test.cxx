/**
 * @file DFOModule_test.cxx Test application that tests and demonstrates
 * the functionality of the DFOModule class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DFOModule.hpp"

#include "appfwk/app/Nljs.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TriggerInhibit.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"
#include "opmonlib/TestOpMonManager.hpp"

#define BOOST_TEST_MODULE DFOModule_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace dunedaq::dfmodules;

namespace dunedaq {


struct EnvFixture
{
  EnvFixture() { setenv("DUNEDAQ_PARTITION", "partition_name", 0); }
};
BOOST_TEST_GLOBAL_FIXTURE(EnvFixture);

struct CfgFixture
{
  CfgFixture()
  {   std::string oksConfig = "oksconflibs:test/config/datafloworchestrator_test.data.xml";
    std::string appName = "TestApp";
    std::string sessionName = "partition_name";
    cfgMgr = std::make_shared<dunedaq::appfwk::ConfigurationManager>(oksConfig, appName, sessionName);
    modCfg  = std::make_shared<dunedaq::appfwk::ModuleConfiguration>(cfgMgr);
    get_iomanager()->configure(modCfg->queues(), modCfg->networkconnections(), false, std::chrono::milliseconds(100), opmgr);
  }
  ~CfgFixture() {
    get_iomanager()->reset();
  }

  dunedaq::opmonlib::TestOpMonManager opmgr;
  std::shared_ptr<dunedaq::appfwk::ConfigurationManager> cfgMgr;
  std::shared_ptr<dunedaq::appfwk::ModuleConfiguration> modCfg;
};

BOOST_FIXTURE_TEST_SUITE(DFOModule_test, CfgFixture)

void
send_init_token(std::string connection_name = "trigdec_0")
{
  dfmessages::TriggerDecisionToken token;
  token.run_number = 0;
  token.trigger_number = 0;
  token.decision_destination = connection_name;

  TLOG() << "Sending Init TriggerDecisionToken to DFO";
  get_iom_sender<dfmessages::TriggerDecisionToken>("token")->send(std::move(token), iomanager::Sender::s_block);
}
void
send_token(dfmessages::trigger_number_t trigger_number,
           std::string connection_name = "trigdec_0",
           bool different_run = false)
{
  dfmessages::TriggerDecisionToken token;
  token.run_number = different_run ? 2 : 1;
  token.trigger_number = trigger_number;
  token.decision_destination = connection_name;

  TLOG() << "Sending TriggerDecisionToken with trigger number " << trigger_number << " to DFO";
  get_iom_sender<dfmessages::TriggerDecisionToken>("token")->send(std::move(token), iomanager::Sender::s_block);
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
  iom->get_sender<dfmessages::TriggerDecision>("trigdec")->send(std::move(td), iomanager::Sender::s_block);
}

BOOST_AUTO_TEST_CASE(CopyAndMoveSemantics)
{
  BOOST_REQUIRE(!std::is_copy_constructible_v<DFOModule>);
  BOOST_REQUIRE(!std::is_copy_assignable_v<DFOModule>);
  BOOST_REQUIRE(!std::is_move_constructible_v<DFOModule>);
  BOOST_REQUIRE(!std::is_move_assignable_v<DFOModule>);
}

BOOST_AUTO_TEST_CASE(Constructor)
{
  auto dfo = appfwk::make_module("DFOModule", "test");
}

BOOST_AUTO_TEST_CASE(Init)
{
  auto dfo = appfwk::make_module("DFOModule", "test");
  dfo->init(modCfg);
}

BOOST_AUTO_TEST_CASE(Commands)
{
  auto dfo = appfwk::make_module("DFOModule", "test");
  opmgr.register_node("dfo", dfo);
  dfo->init(modCfg);

  auto conf_json = "{\"thresholds\": { \"free\": 1, \"busy\": 2 }, "
                   "\"general_queue_timeout\": 100, \"td_send_retries\": 5}"_json;
  auto start_json = "{\"run\": 1}"_json;
  auto null_json = "{}"_json;

  dfo->execute_command("conf", "INITIAL", conf_json);
  dfo->execute_command("start", "CONFIGURED", start_json);
  dfo->execute_command("drain_dataflow", "RUNNING", null_json);
  dfo->execute_command("scrap", "CONFIGURED", null_json);

  opmgr.collect();
  auto opmon_facility = opmgr.get_backend_facility();
  auto list = opmon_facility->get_entries(std::regex(".*DFOInfo"));
  BOOST_REQUIRE_EQUAL(list.size(), 1);
  const auto & entry = list.front();
  const auto & payload = entry.data();
  BOOST_REQUIRE_EQUAL(payload.find("tokens_received")->second.uint8_value(), 0);
  BOOST_REQUIRE_EQUAL(payload.find("decisions_received")->second.uint8_value(), 0);
  BOOST_REQUIRE_EQUAL(payload.find("decisions_sent")->second.uint8_value(), 0);
  BOOST_REQUIRE_EQUAL(payload.find("forwarding_decision")->second.uint8_value(), 0);
  BOOST_REQUIRE_EQUAL(payload.find("waiting_for_decision")->second.uint8_value(), 0);
  BOOST_REQUIRE_EQUAL(payload.find("deciding_destination")->second.uint8_value(), 0);
  BOOST_REQUIRE_EQUAL(payload.find("waiting_for_token")->second.uint8_value(), 0);
  BOOST_REQUIRE_EQUAL(payload.find("processing_token")->second.uint8_value(), 0);
  
}

BOOST_AUTO_TEST_CASE(DataFlow)
{
  auto dfo = appfwk::make_module("DFOModule", "test");
  opmgr.register_node("dfo", dfo);
  dfo->init(modCfg);

  auto conf_json = "{\"thresholds\": { \"free\": 1, \"busy\": 2 }, "
                   "\"general_queue_timeout\": 100, \"td_send_retries\": 5}"_json;
  auto start_json = "{\"run\": 1}"_json;
  auto null_json = "{}"_json;

  dfo->execute_command("conf", "INITIAL", conf_json);

  auto iom = iomanager::IOManager::get();
  auto dec_recv = iom->get_receiver<dfmessages::TriggerDecision>("trigdec_0");
  dec_recv->add_callback(recv_trigdec);
  auto inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("triginh");
  inh_recv->add_callback(recv_triginh);

  send_trigdec(1, true);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  send_token(999, "trigdec_0", true);
  send_token(9999, "trigdec_0", true);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  opmgr.collect();
  // Note: Counters are reset by calling collect!
  auto opmon_facility = opmgr.get_backend_facility();
  auto list = opmon_facility->get_entries(std::regex(".*DFOInfo"));
  BOOST_REQUIRE_EQUAL(list.size(), 1);
  auto entry = list.front();
  auto payload = entry.data();
  
  BOOST_REQUIRE_EQUAL(payload.find("tokens_received")->second.uint8_value(), 0);

  dfo->execute_command("start", "CONFIGURED", start_json);
  send_init_token();

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  opmgr.collect();
  list = opmon_facility->get_entries(std::regex(".*DFOInfo"));
  BOOST_REQUIRE_EQUAL(list.size(), 1);
  entry = list.front();
  payload = entry.data();

  BOOST_REQUIRE_EQUAL(payload.find("tokens_received")->second.uint8_value(), 0);
  BOOST_REQUIRE_EQUAL(payload.find("decisions_received")->second.uint8_value(), 0);
  BOOST_REQUIRE_EQUAL(payload.find("decisions_sent")->second.uint8_value(), 0);

  send_trigdec(2);
  send_trigdec(3);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  send_trigdec(4);

  opmgr.collect();
  list = opmon_facility->get_entries(std::regex(".*DFOInfo"));
  BOOST_REQUIRE_EQUAL(list.size(), 1);
  entry = list.front();
  payload = entry.data();
  BOOST_REQUIRE_EQUAL(payload.find("tokens_received")->second.uint8_value(), 0);
  BOOST_REQUIRE_EQUAL(payload.find("decisions_received")->second.uint8_value(), 2);
  BOOST_REQUIRE_EQUAL(payload.find("decisions_sent")->second.uint8_value(), 2);

  BOOST_REQUIRE(busy_signal_recvd.load());
  std::this_thread::sleep_for(std::chrono::milliseconds(400));

  opmgr.collect();
  list = opmon_facility->get_entries(std::regex(".*DFOInfo"));
  BOOST_REQUIRE_EQUAL(list.size(), 1);
  entry = list.front();
  payload = entry.data();
  BOOST_REQUIRE_EQUAL(payload.find("tokens_received")->second.uint8_value(), 3);
  BOOST_REQUIRE_EQUAL(payload.find("decisions_received")->second.uint8_value(), 1);
  BOOST_REQUIRE_EQUAL(payload.find("decisions_sent")->second.uint8_value(), 1);
  BOOST_REQUIRE(!busy_signal_recvd.load());

  dfo->execute_command("drain_dataflow", "RUNNING", null_json);
  dfo->execute_command("scrap", "CONFIGURED", null_json);

  dec_recv->remove_callback();
  inh_recv->remove_callback();
}

BOOST_AUTO_TEST_CASE(SendTrigDecFailed)
{
  auto dfo = appfwk::make_module("DFOModule", "test");
  dfo->init(modCfg);

  auto conf_json = "{\"thresholds\": { \"free\": 1, \"busy\": 2 }, "
                   "\"general_queue_timeout\": 100, \"td_send_retries\": 5}"_json;
  auto start_json = "{\"run\": 1}"_json;
  auto null_json = "{}"_json;

  dfo->execute_command("conf", "INITIAL", conf_json);

  dfo->execute_command("start", "CONFIGURED", start_json);

  send_init_token("invalid_connection");

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  send_trigdec(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  // auto info = get_dfo_info(dfo);
  // BOOST_REQUIRE_EQUAL(info.tokens_received, 0);
  // BOOST_REQUIRE_EQUAL(info.decisions_received, 1);
  // BOOST_REQUIRE_EQUAL(info.decisions_sent, 0);

  // FWIW, tell the DFO to retry the invalid connection
  send_token(1000, "invalid_connection");
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Token for unknown dataflow app
  send_token(1000);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  dfo->execute_command("drain_dataflow", "RUNNING", null_json);
  dfo->execute_command("scrap", "CONFIGURED", null_json);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace dunedaq
