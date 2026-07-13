/**
 * @file DataflowStatusModule_test.cxx Test application that tests and demonstrates
 * the functionality of the DataflowStatusModule class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DataflowStatusModule.hpp"

#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TriggerInhibit.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/opmon/DataflowStatusModule.pb.h"
#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"
#include "opmonlib/TestOpMonManager.hpp"

#define BOOST_TEST_MODULE DataflowStatusModule_test // NOLINT

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
  {
    std::string oksConfig = "oksconflibs:test/config/dataflowstatus_test.data.xml";
    std::string appName = "TestApp";
    std::string sessionName = "partition_name";
    cfgMgr = std::make_shared<appfwk::ConfigurationManager>(oksConfig, appName, sessionName);
    get_iomanager()->configure(sessionName, cfgMgr->get_queues(), cfgMgr->get_networkconnections(), nullptr, opmgr);
  }
  ~CfgFixture() { get_iomanager()->reset(); }

  auto get_opmon_info()
  {

    opmgr.collect();
    auto opmon_facility = opmgr.get_backend_facility();
    auto list = opmon_facility->get_entries(std::regex(".*DataflowStatusInfo"));
    BOOST_REQUIRE_EQUAL(list.size(), 1);
    const auto& entry = list.front();
    return opmonlib::from_entry<dfmodules::opmon::DataflowStatusInfo>(entry);
  }

  opmonlib::TestOpMonManager opmgr;
  std::shared_ptr<appfwk::ConfigurationManager> cfgMgr;
};

struct ConnectionFixture
{
  ConnectionFixture() = default;

  static void send_trigdec(dfmessages::trigger_number_t trigger_number, bool different_run = false)
  {
    dfmessages::TriggerDecision td;
    td.trigger_number = trigger_number;
    td.run_number = different_run ? 2 : 1;
    td.trigger_timestamp = 1;
    td.trigger_type = 1;
    td.readout_type = dfmessages::ReadoutType::kLocalized;
    auto iom = iomanager::IOManager::get();
    TLOG() << "Sending TriggerDecision with trigger number " << trigger_number << " from \"DFO\"";
    iom->get_sender<dfmessages::TriggerDecision>("trigdec_dfo")->send(std::move(td), iomanager::Sender::s_block);
  }

  static void send_token(
    dfmessages::trigger_number_t trigger_number,
    dfmessages::sequence_number_t sequence_number = 0,
    bool different_run = false)
  {
    dfmessages::TriggerDecisionToken token;
    token.trigger_id = dfmessages::TriggerId{ different_run ? 2U : 1U, trigger_number, sequence_number };
    token.writer_identifier = "test_writer";
    token.data_size = 1234;
    auto iom = iomanager::IOManager::get();
    auto sender = iom->get_sender<dfmessages::TriggerDecisionToken>("token");
    sender->send(std::move(token), iomanager::Sender::s_block);
  }

  static void send_dataflow_status_request(dfmessages::trigger_number_t trigger_number, bool different_run = false)
  {
    dfmessages::DataflowStatusRequest request;
    request.trigger_number = trigger_number;
    request.run_number = different_run ? 2 : 1;
    request.reply_destination = "df_status";
    auto iom = iomanager::IOManager::get();
    auto sender = iom->get_sender<dfmessages::DataflowStatusRequest>("df_status_request");
    sender->send(std::move(request), iomanager::Sender::s_block);
  }

  static void send_trb_completion(
    dfmessages::trigger_number_t trigger_number,
    dfmessages::sequence_number_t sequence_number = 0,
    size_t trigger_record_max_sequence_number = 0,
    bool different_run = false)
  {
    dfmessages::TRBCompletion trb_complete;
    trb_complete.trigger_id = dfmessages::TriggerId{ different_run ? 2U : 1U, trigger_number, sequence_number };
    trb_complete.source_id = daqdataformats::SourceID(daqdataformats::SourceID::Subsystem::kTRBuilder, 1);
    trb_complete.trigger_record_max_sequence_number = trigger_record_max_sequence_number;
    auto iom = iomanager::IOManager::get();
    auto sender = iom->get_sender<dfmessages::TRBCompletion>("trb_completion");
    sender->send(std::move(trb_complete), iomanager::Sender::s_block);
  }

  static std::vector<dfmessages::DataflowStatus> s_received_statuses;
  static void receive_dataflow_status(const dfmessages::DataflowStatus& status)
  {
    TLOG() << "Received DataflowStatus with trigger number " << status.trigger_number;
    s_received_statuses.push_back(status);
  }
  static std::vector<dfmessages::TriggerDecision> s_received_decisions;
  static void receive_trigger_decision(const dfmessages::TriggerDecision& decision)
  {
    TLOG() << "Received TriggerDecision with trigger number " << decision.trigger_number;
    s_received_decisions.push_back(decision);
  }

  static void reset()
  {
    s_received_statuses.clear();
    s_received_decisions.clear();
  }
};
std::vector<dfmessages::DataflowStatus> ConnectionFixture::s_received_statuses;
std::vector<dfmessages::TriggerDecision> ConnectionFixture::s_received_decisions;

BOOST_FIXTURE_TEST_SUITE(DataflowStatusModule_test, CfgFixture)

BOOST_AUTO_TEST_CASE(CopyAndMoveSemantics)
{
  BOOST_REQUIRE(!std::is_copy_constructible_v<DataflowStatusModule>);
  BOOST_REQUIRE(!std::is_copy_assignable_v<DataflowStatusModule>);
  BOOST_REQUIRE(!std::is_move_constructible_v<DataflowStatusModule>);
  BOOST_REQUIRE(!std::is_move_assignable_v<DataflowStatusModule>);
}

BOOST_AUTO_TEST_CASE(Constructor)
{
  TLOG() << "Test case Constructor BEGIN";
  auto dfs = appfwk::make_module("DataflowStatusModule", "test");
  TLOG() << "Test case Constructor END";
}

BOOST_AUTO_TEST_CASE(Init)
{
  TLOG() << "Test case Init BEGIN";
  auto dfs = appfwk::make_module("DataflowStatusModule", "test");
  dfs->init(cfgMgr);
  TLOG() << "Test case Init END";
}

BOOST_AUTO_TEST_CASE(Commands)
{
  TLOG() << "Test case Commands BEGIN";
  auto dfs = appfwk::make_module("DataflowStatusModule", "test");
  opmgr.register_node("dfs", dfs);
  dfs->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  auto iom = iomanager::IOManager::get();
  ConnectionFixture::reset();
  auto dfs_recv = iom->get_receiver<dfmessages::DataflowStatus>("df_status");
  dfs_recv->add_callback(ConnectionFixture::receive_dataflow_status);
  auto dec_recv = iom->get_receiver<dfmessages::TriggerDecision>("trigdec_trb");
  dec_recv->add_callback(ConnectionFixture::receive_trigger_decision);

  TLOG() << "Executing conf command";
  dfs->execute_command("conf", null_data);
  TLOG() << "Executing start command";
  dfs->execute_command("start", start_data);

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  TLOG() << "Executing stop command";
  dfs->execute_command("stop", null_data);
  TLOG() << "Executing scrap command";
  dfs->execute_command("scrap", null_data);

  TLOG() << "Retrieving metrics from opmon";
  auto metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.requests_received(), 0);
  BOOST_REQUIRE(metric.status_messages_sent() >= 1);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);

  BOOST_REQUIRE_EQUAL(ConnectionFixture::s_received_statuses.back().decision_destination, "trigdec_dfo");

  TLOG() << "Test case Commands END";
}

BOOST_AUTO_TEST_CASE(DataFlow)
{
  TLOG() << "Test case DataFlow BEGIN";
  auto dfs = appfwk::make_module("DataflowStatusModule", "test");
  opmgr.register_node("dfs", dfs);
  dfs->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  auto iom = iomanager::IOManager::get();
  ConnectionFixture::reset();
  auto dfs_recv = iom->get_receiver<dfmessages::DataflowStatus>("df_status");
  dfs_recv->add_callback(ConnectionFixture::receive_dataflow_status);
  auto dec_recv = iom->get_receiver<dfmessages::TriggerDecision>("trigdec_trb");
  dec_recv->add_callback(ConnectionFixture::receive_trigger_decision);

  dfs->execute_command("conf", null_data);

  ConnectionFixture::send_trigdec(1, true);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  ConnectionFixture::send_token(999, true);
  ConnectionFixture::send_token(9999, true);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Note: Counters are reset by calling get_opmon_info!
  auto metric = get_opmon_info();

  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);

  dfs->execute_command("start", start_data);

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);

  ConnectionFixture::send_trigdec(2);
  ConnectionFixture::send_trigdec(3);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  BOOST_REQUIRE(ConnectionFixture::s_received_decisions.size() == 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.size() >= 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(2) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(3) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  ConnectionFixture::send_trigdec(4);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 2);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 2);

  std::this_thread::sleep_for(std::chrono::milliseconds(400));

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 1);

  ConnectionFixture::send_trb_completion(3);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(2) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(4) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.count(3) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);

  ConnectionFixture::send_token(3);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(2) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(4) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.count(3) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 1);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 1);

  ConnectionFixture::send_trb_completion(2);
  ConnectionFixture::send_trb_completion(4);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.count(2) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.count(4) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.count(3) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 1);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 2);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);

  ConnectionFixture::send_token(2);
  ConnectionFixture::send_token(4);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 3);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.count(2) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.count(3) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.count(4) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 3);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 2);

  auto start_time = std::chrono::steady_clock::now();
  dfs->execute_command("stop", null_data);
  auto stop_time = std::chrono::steady_clock::now();
  BOOST_REQUIRE(stop_time - start_time < std::chrono::milliseconds(100));

  dfs->execute_command("scrap", null_data);

  dec_recv->remove_callback();
  dfs_recv->remove_callback();
  TLOG() << "Test case DataFlow END";
}

BOOST_AUTO_TEST_CASE(Requests)
{
  TLOG() << "Test case Requests BEGIN";
  auto dfs = appfwk::make_module("DataflowStatusModule", "test");
  opmgr.register_node("dfs", dfs);
  dfs->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  auto iom = iomanager::IOManager::get();
  ConnectionFixture::reset();
  auto dfs_recv = iom->get_receiver<dfmessages::DataflowStatus>("df_status");
  dfs_recv->add_callback(ConnectionFixture::receive_dataflow_status);
  auto dec_recv = iom->get_receiver<dfmessages::TriggerDecision>("trigdec_trb");
  dec_recv->add_callback(ConnectionFixture::receive_trigger_decision);

  dfs->execute_command("conf", null_data);

  // Note: Counters are reset by calling get_opmon_info!
  auto metric = get_opmon_info();

  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);

  dfs->execute_command("start", start_data);

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);

  // Requests for different run are ignored
  ConnectionFixture::send_dataflow_status_request(1, true);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.requests_received(), 0);

  ConnectionFixture::send_dataflow_status_request(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  dunedaq::dfmessages::DataflowStatus expected_status;
  for (const auto& status : ConnectionFixture::s_received_statuses) {
    if (status.trigger_number == 1) {
      expected_status = status;
      break;
    }
  }

  BOOST_REQUIRE(expected_status.trigger_number != dfmessages::TypeDefaults::s_invalid_trigger_number);
  BOOST_REQUIRE_EQUAL(expected_status.trigger_number, 1);
  BOOST_REQUIRE_EQUAL(expected_status.run_number, 1);
  BOOST_REQUIRE_EQUAL(expected_status.triggers_building.size(), 0);
  BOOST_REQUIRE_EQUAL(expected_status.triggers_writing.size(), 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.requests_received(), 1);

  dfs->execute_command("stop", null_data);
  dfs->execute_command("scrap", null_data);

  dec_recv->remove_callback();
  dfs_recv->remove_callback();
  TLOG() << "Test case Requests END";
}

BOOST_AUTO_TEST_CASE(OutOfOrder)
{
  TLOG() << "Test case OutOfOrder BEGIN";
  auto dfs = appfwk::make_module("DataflowStatusModule", "test");
  opmgr.register_node("dfs", dfs);
  dfs->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  auto iom = iomanager::IOManager::get();
  ConnectionFixture::reset();
  auto dfs_recv = iom->get_receiver<dfmessages::DataflowStatus>("df_status");
  dfs_recv->add_callback(ConnectionFixture::receive_dataflow_status);
  auto dec_recv = iom->get_receiver<dfmessages::TriggerDecision>("trigdec_trb");
  dec_recv->add_callback(ConnectionFixture::receive_trigger_decision);

  dfs->execute_command("conf", null_data);

  // Note: Counters are reset by calling get_opmon_info!
  auto metric = get_opmon_info();

  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);

  dfs->execute_command("start", start_data);

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);

  ConnectionFixture::send_trb_completion(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.unexpected_trb_completions_received(), 1);

  ConnectionFixture::send_token(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);

  BOOST_REQUIRE_EQUAL(metric.unexpected_trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.unexpected_tokens_received(), 1);

  ConnectionFixture::send_trigdec(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  BOOST_REQUIRE(ConnectionFixture::s_received_decisions.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.size() >= 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(1) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 1);
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.unexpected_trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.unexpected_tokens_received(), 0);

  ConnectionFixture::send_trigdec(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  BOOST_REQUIRE(ConnectionFixture::s_received_decisions.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.size() >= 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(1) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);

  BOOST_REQUIRE_EQUAL(metric.duplicate_decisions_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.unexpected_trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.unexpected_tokens_received(), 0);

  ConnectionFixture::send_token(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  BOOST_REQUIRE(ConnectionFixture::s_received_decisions.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.size() >= 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.count(1) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 1);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 1);

  BOOST_REQUIRE_EQUAL(metric.duplicate_decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.unexpected_trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.unexpected_tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.early_tokens_received(), 1);

  ConnectionFixture::send_trb_completion(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  BOOST_REQUIRE(ConnectionFixture::s_received_decisions.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.size() >= 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.count(1) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 1);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);

  BOOST_REQUIRE_EQUAL(metric.duplicate_decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.unexpected_trb_completions_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.unexpected_tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.early_tokens_received(), 0);

  dfs->execute_command("stop", null_data);
  dfs->execute_command("scrap", null_data);

  dec_recv->remove_callback();
  dfs_recv->remove_callback();
  TLOG() << "Test case OutOfOrder END";
}

BOOST_AUTO_TEST_CASE(StopTimeout_Building)
{
  TLOG() << "Test case StopTimeout_Building BEGIN";
  auto dfs = appfwk::make_module("DataflowStatusModule", "test");
  opmgr.register_node("dfs", dfs);
  dfs->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  auto iom = iomanager::IOManager::get();
  ConnectionFixture::reset();
  auto dfs_recv = iom->get_receiver<dfmessages::DataflowStatus>("df_status");
  dfs_recv->add_callback(ConnectionFixture::receive_dataflow_status);
  auto dec_recv = iom->get_receiver<dfmessages::TriggerDecision>("trigdec_trb");
  dec_recv->add_callback(ConnectionFixture::receive_trigger_decision);

  dfs->execute_command("conf", null_data);
  dfs->execute_command("start", start_data);

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  auto metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);

  ConnectionFixture::send_trigdec(2);
  ConnectionFixture::send_trigdec(3);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  BOOST_REQUIRE(ConnectionFixture::s_received_decisions.size() == 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.size() >= 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(2) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(3) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 2);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 2);

  auto start_time = std::chrono::steady_clock::now();
  dfs->execute_command("stop", null_data);
  auto stop_time = std::chrono::steady_clock::now();
  BOOST_REQUIRE(stop_time - start_time > std::chrono::milliseconds(1000));

  dfs->execute_command("scrap", null_data);

  dec_recv->remove_callback();
  dfs_recv->remove_callback();
  TLOG() << "Test case StopTimeout_Building END";
}

BOOST_AUTO_TEST_CASE(StopTimeout_Writing)
{
  TLOG() << "Test case StopTimeout_Writing BEGIN";
  auto dfs = appfwk::make_module("DataflowStatusModule", "test");
  opmgr.register_node("dfs", dfs);
  dfs->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  auto iom = iomanager::IOManager::get();
  ConnectionFixture::reset();
  auto dfs_recv = iom->get_receiver<dfmessages::DataflowStatus>("df_status");
  dfs_recv->add_callback(ConnectionFixture::receive_dataflow_status);
  auto dec_recv = iom->get_receiver<dfmessages::TriggerDecision>("trigdec_trb");
  dec_recv->add_callback(ConnectionFixture::receive_trigger_decision);

  dfs->execute_command("conf", null_data);

  dfs->execute_command("start", start_data);

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  auto metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);

  ConnectionFixture::send_trigdec(2);
  ConnectionFixture::send_trigdec(3);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  BOOST_REQUIRE(ConnectionFixture::s_received_decisions.size() == 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.size() >= 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(2) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(3) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 2);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 2);

  ConnectionFixture::send_trb_completion(2);
  ConnectionFixture::send_trb_completion(3);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.count(2) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.count(3) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 2);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);

  auto start_time = std::chrono::steady_clock::now();
  dfs->execute_command("stop", null_data);
  auto stop_time = std::chrono::steady_clock::now();
  BOOST_REQUIRE(stop_time - start_time > std::chrono::milliseconds(1000));

  dfs->execute_command("scrap", null_data);

  dec_recv->remove_callback();
  dfs_recv->remove_callback();
  TLOG() << "Test case StopTimeout_Writing END";
}

BOOST_AUTO_TEST_CASE(StopTimeout_BuildingAndWriting)
{
  TLOG() << "Test case StopTimeout_BuildingAndWriting BEGIN";
  auto dfs = appfwk::make_module("DataflowStatusModule", "test");
  opmgr.register_node("dfs", dfs);
  dfs->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  auto iom = iomanager::IOManager::get();
  ConnectionFixture::reset();
  auto dfs_recv = iom->get_receiver<dfmessages::DataflowStatus>("df_status");
  dfs_recv->add_callback(ConnectionFixture::receive_dataflow_status);
  auto dec_recv = iom->get_receiver<dfmessages::TriggerDecision>("trigdec_trb");
  dec_recv->add_callback(ConnectionFixture::receive_trigger_decision);

  dfs->execute_command("conf", null_data);

  dfs->execute_command("start", start_data);

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  auto metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);

  ConnectionFixture::send_trigdec(2);
  ConnectionFixture::send_trigdec(3);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  BOOST_REQUIRE(ConnectionFixture::s_received_decisions.size() == 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.size() >= 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 2);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(2) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(3) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 2);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 2);

  ConnectionFixture::send_trb_completion(2);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(3) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.count(2) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);

  auto start_time = std::chrono::steady_clock::now();
  dfs->execute_command("stop", null_data);
  auto stop_time = std::chrono::steady_clock::now();

  // Stop transition timeout is shared between waiting for both building and writting triggers
  BOOST_REQUIRE(stop_time - start_time > std::chrono::milliseconds(1000));
  BOOST_REQUIRE(stop_time - start_time < std::chrono::milliseconds(2000));

  dfs->execute_command("scrap", null_data);

  dec_recv->remove_callback();
  dfs_recv->remove_callback();
  TLOG() << "Test case StopTimeout_BuildingAndWriting END";
}

BOOST_AUTO_TEST_CASE(MultipleSequences)
{
  TLOG() << "Test case MultipleSequences BEGIN";
  auto dfs = appfwk::make_module("DataflowStatusModule", "test");
  opmgr.register_node("dfs", dfs);
  dfs->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  auto iom = iomanager::IOManager::get();
  ConnectionFixture::reset();
  auto dfs_recv = iom->get_receiver<dfmessages::DataflowStatus>("df_status");
  dfs_recv->add_callback(ConnectionFixture::receive_dataflow_status);
  auto dec_recv = iom->get_receiver<dfmessages::TriggerDecision>("trigdec_trb");
  dec_recv->add_callback(ConnectionFixture::receive_trigger_decision);

  dfs->execute_command("conf", null_data);
  dfs->execute_command("start", start_data);

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  auto metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);

  // Send a trigger decision that will be split into multiple sequences (e.g., sequence 0, 1, 2)
  ConnectionFixture::send_trigdec(100);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  BOOST_REQUIRE(ConnectionFixture::s_received_decisions.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.size() >= 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(100) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 1);

  // Send first TRB completion for sequence 0, with max_sequence_number = 2
  ConnectionFixture::send_trb_completion(100, 0, 2);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  // Should still be building, waiting for all sequences to complete
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(100) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);

  // Send second TRB completion for sequence 1
  ConnectionFixture::send_trb_completion(100, 1, 2);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  // Still building, waiting for sequence 2
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.count(100) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);

  // Send third TRB completion for sequence 2 - all sequences now built
  ConnectionFixture::send_trb_completion(100, 2, 2);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  // Now should transition to writing state
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.count(100) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);

  // Send first token for sequence 0
  ConnectionFixture::send_token(100, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  // Still writing, waiting for all tokens
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.count(100) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 1);

  // Send second token for sequence 1
  ConnectionFixture::send_token(100, 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  // Still writing, waiting for token for sequence 2
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.count(100) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 0);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 1);

  // Send third token for sequence 2 - all sequences now written
  ConnectionFixture::send_token(100, 2);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  // Now should be completed
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_building.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().triggers_writing.size() == 0);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.size() == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().recently_completed_triggers.count(100) == 1);
  BOOST_REQUIRE(ConnectionFixture::s_received_statuses.back().trigger_records_processed == 1);

  metric = get_opmon_info();
  BOOST_REQUIRE_EQUAL(metric.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 1);

  dfs->execute_command("stop", null_data);
  dfs->execute_command("scrap", null_data);

  dec_recv->remove_callback();
  dfs_recv->remove_callback();
  TLOG() << "Test case MultipleSequences END";
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace dunedaq
