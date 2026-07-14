/**
 * @file DFOProtocol_test.cxx Integration test for the DFO protocol
 * with multiple DFO and DataflowStatus modules.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DFOModule.hpp"
#include "DataflowStatusModule.hpp"

#include "dfmessages/DataflowStatus.hpp"
#include "dfmessages/DataflowStatusRequest.hpp"
#include "dfmessages/TRBCompletion.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TriggerInhibit.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/opmon/DFOModule.pb.h"
#include "dfmodules/opmon/DataflowStatusModule.pb.h"
#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"
#include "opmonlib/TestOpMonManager.hpp"

#define BOOST_TEST_MODULE DFOProtocol_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
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
    std::string oksConfig = "oksconflibs:test/config/dfoprotocol_test.data.xml";
    std::string appName = "TestApp";
    std::string sessionName = "partition_name";
    cfgMgr = std::make_shared<dunedaq::appfwk::ConfigurationManager>(oksConfig, appName, sessionName);
    get_iomanager()->configure(sessionName, cfgMgr->get_queues(), cfgMgr->get_networkconnections(), nullptr, opmgr);
  }
  ~CfgFixture() { get_iomanager()->reset(); }

  void collect_opmon_entries()
  {
    opmgr.collect();
    auto opmon_facility = opmgr.get_backend_facility();
    entries = opmon_facility->get_entries(std::regex(".*(DFOInfo|DataflowStatusInfo)"));
    BOOST_REQUIRE_EQUAL(entries.size(), 6);
  }

  std::unordered_map<std::string, dfmodules::opmon::DFOInfo> get_dfo_info()
  {
    auto entry = entries.begin();
    std::unordered_map<std::string, dfmodules::opmon::DFOInfo> output;
    for (; entry != entries.end(); ++entry) {
      if (entry->measurement() != "dunedaq.dfmodules.opmon.DFOInfo") {
        continue;
      }
      auto name = entry->origin().substructure()[0];
      output[name] = opmonlib::from_entry<dfmodules::opmon::DFOInfo>(*entry);
    }

    BOOST_REQUIRE_EQUAL(output.size(), 3);
    return output;
  }

  std::unordered_map<std::string, dfmodules::opmon::DataflowStatusInfo> get_dfs_info()
  {
    auto entry = entries.begin();
    std::unordered_map<std::string, dfmodules::opmon::DataflowStatusInfo> output;
    for (; entry != entries.end(); ++entry) {
      if (entry->measurement() != "dunedaq.dfmodules.opmon.DataflowStatusInfo") {
        continue;
      }
      auto name = entry->origin().substructure()[0];
      output[name] = opmonlib::from_entry<dfmodules::opmon::DataflowStatusInfo>(*entry);
    }

    BOOST_REQUIRE_EQUAL(output.size(), 3);
    return output;
  }

  std::list<dunedaq::opmon::OpMonEntry> entries;
  dunedaq::opmonlib::TestOpMonManager opmgr;
  std::shared_ptr<dunedaq::appfwk::ConfigurationManager> cfgMgr;
};

BOOST_FIXTURE_TEST_SUITE(DFOProtocol_test, CfgFixture)

// Thread-safe collectors for received messages
struct MessageCollector
{
  std::mutex mutex;
  std::vector<dfmessages::TriggerDecision> trigger_decisions;
  std::vector<dfmessages::TriggerInhibit> trigger_inhibits;
  std::map<std::string, std::atomic<bool>> latest_inhibit_state;

  void reset()
  {
    std::lock_guard<std::mutex> lock(mutex);
    trigger_decisions.clear();
    trigger_inhibits.clear();
    latest_inhibit_state.clear();
  }

  void add_trigger_decision(const dfmessages::TriggerDecision& decision)
  {
    std::lock_guard<std::mutex> lock(mutex);
    trigger_decisions.push_back(decision);
  }

  void add_trigger_inhibit(const std::string& source, const dfmessages::TriggerInhibit& inhibit)
  {
    std::lock_guard<std::mutex> lock(mutex);
    trigger_inhibits.push_back(inhibit);
    latest_inhibit_state[source].store(inhibit.busy);
  }

  std::vector<dfmessages::TriggerDecision> get_decisions()
  {
    std::lock_guard<std::mutex> lock(mutex);
    return trigger_decisions;
  }

  std::vector<dfmessages::TriggerInhibit> get_inhibits()
  {
    std::lock_guard<std::mutex> lock(mutex);
    return trigger_inhibits;
  }

  bool get_latest_inhibit(const std::string& source) { return latest_inhibit_state[source].load(); }

  size_t count_decisions()
  {
    std::lock_guard<std::mutex> lock(mutex);
    return trigger_decisions.size();
  }
};

static MessageCollector df1_collector;
static MessageCollector df2_collector;
static MessageCollector df3_collector;
static MessageCollector dfo_collector;

static void
df1_recv_trigdec(const dfmessages::TriggerDecision& decision)
{
  TLOG() << "DF1 received TriggerDecision " << decision.trigger_number;
  df1_collector.add_trigger_decision(decision);
}

static void
df2_recv_trigdec(const dfmessages::TriggerDecision& decision)
{
  TLOG() << "DF2 received TriggerDecision " << decision.trigger_number;
  df2_collector.add_trigger_decision(decision);
}

static void
df3_recv_trigdec(const dfmessages::TriggerDecision& decision)
{
  TLOG() << "DF3 received TriggerDecision " << decision.trigger_number;
  df3_collector.add_trigger_decision(decision);
}

static void
dfo1_recv_inhibit(const dfmessages::TriggerInhibit& inhibit)
{
  TLOG() << "DFO1 sent TriggerInhibit: busy=" << std::boolalpha << inhibit.busy;
  dfo_collector.add_trigger_inhibit("dfo1", inhibit);
}

static void
dfo2_recv_inhibit(const dfmessages::TriggerInhibit& inhibit)
{
  TLOG() << "DFO2 sent TriggerInhibit: busy=" << std::boolalpha << inhibit.busy;
  dfo_collector.add_trigger_inhibit("dfo2", inhibit);
}

static void
dfo3_recv_inhibit(const dfmessages::TriggerInhibit& inhibit)
{
  TLOG() << "DFO3 sent TriggerInhibit: busy=" << std::boolalpha << inhibit.busy;
  dfo_collector.add_trigger_inhibit("dfo3", inhibit);
}

static void
send_trigdec_to_all_dfos(dfmessages::trigger_number_t trigger_number, uint32_t run_number = 1)
{
  auto iom = iomanager::IOManager::get();

  for (const auto& conn : { "dfo1_trigdec_in", "dfo2_trigdec_in", "dfo3_trigdec_in" }) {
    dfmessages::TriggerDecision td;
    td.trigger_number = trigger_number;
    td.run_number = run_number;
    td.trigger_timestamp = trigger_number * 1000;
    td.trigger_type = 1;
    td.readout_type = dfmessages::ReadoutType::kLocalized;

    auto sender = iom->get_sender<dfmessages::TriggerDecision>(conn);
    sender->send(std::move(td), iomanager::Sender::s_block);
  }
  TLOG() << "Sent TriggerDecision " << trigger_number << " to all DFOs";
}

static void
send_trb_completion(const std::string& df_module,
                    dfmessages::trigger_number_t trigger_number,
                    dfmessages::sequence_number_t sequence_number = 0,
                    size_t max_sequence_number = 0)
{
  auto iom = iomanager::IOManager::get();

  dfmessages::TRBCompletion completion;
  completion.trigger_id = dfmessages::TriggerId{ 1, trigger_number, sequence_number };
  completion.source_id = daqdataformats::SourceID(daqdataformats::SourceID::Subsystem::kTRBuilder, 1);
  completion.trigger_record_max_sequence_number = max_sequence_number;

  auto sender = iom->get_sender<dfmessages::TRBCompletion>(df_module + "_trb_completion_in");
  sender->send(std::move(completion), iomanager::Sender::s_block);
  TLOG() << "Sent TRBCompletion for trigger " << trigger_number << " to " << df_module;
}

static void
send_token(const std::string& df_module,
           dfmessages::trigger_number_t trigger_number,
           dfmessages::sequence_number_t sequence_number = 0)
{
  auto iom = iomanager::IOManager::get();

  dfmessages::TriggerDecisionToken token;
  token.trigger_id = dfmessages::TriggerId{ 1, trigger_number, sequence_number };
  token.writer_identifier = "test_writer";
  token.data_size = 1234;

  auto sender = iom->get_sender<dfmessages::TriggerDecisionToken>(df_module + "_token_in");
  sender->send(std::move(token), iomanager::Sender::s_block);
  TLOG() << "Sent Token for trigger " << trigger_number << " to " << df_module;
}

BOOST_AUTO_TEST_CASE(StableAlgorithmOptimalConditions)
{
  TLOG() << "Test case StableAlgorithmOptimalConditions BEGIN";

  // Create modules
  auto dfo1 = appfwk::make_module("DFOModule", "dfo1");
  auto dfo2 = appfwk::make_module("DFOModule", "dfo2");
  auto dfo3 = appfwk::make_module("DFOModule", "dfo3");
  auto df1 = appfwk::make_module("DataflowStatusModule", "df1");
  auto df2 = appfwk::make_module("DataflowStatusModule", "df2");
  auto df3 = appfwk::make_module("DataflowStatusModule", "df3");

  // Register with OpMon
  opmgr.register_node("dfo1", dfo1);
  opmgr.register_node("dfo2", dfo2);
  opmgr.register_node("dfo3", dfo3);
  opmgr.register_node("df1", df1);
  opmgr.register_node("df2", df2);
  opmgr.register_node("df3", df3);

  // Initialize all modules
  dfo1->init(cfgMgr);
  dfo2->init(cfgMgr);
  dfo3->init(cfgMgr);
  df1->init(cfgMgr);
  df2->init(cfgMgr);
  df3->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  // Configure all modules
  dfo1->execute_command("conf", null_data);
  dfo2->execute_command("conf", null_data);
  dfo3->execute_command("conf", null_data);
  df1->execute_command("conf", null_data);
  df2->execute_command("conf", null_data);
  df3->execute_command("conf", null_data);

  // Set up receivers
  auto iom = iomanager::IOManager::get();
  df1_collector.reset();
  df2_collector.reset();
  df3_collector.reset();
  dfo_collector.reset();

  auto df1_recv = iom->get_receiver<dfmessages::TriggerDecision>("df1_trigdec_out");
  df1_recv->add_callback(df1_recv_trigdec);
  auto df2_recv = iom->get_receiver<dfmessages::TriggerDecision>("df2_trigdec_out");
  df2_recv->add_callback(df2_recv_trigdec);
  auto df3_recv = iom->get_receiver<dfmessages::TriggerDecision>("df3_trigdec_out");
  df3_recv->add_callback(df3_recv_trigdec);

  auto dfo1_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo1_triginh_out");
  dfo1_inh_recv->add_callback(dfo1_recv_inhibit);
  auto dfo2_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo2_triginh_out");
  dfo2_inh_recv->add_callback(dfo2_recv_inhibit);
  auto dfo3_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo3_triginh_out");
  dfo3_inh_recv->add_callback(dfo3_recv_inhibit);

  // Start all modules
  dfo1->execute_command("start", start_data);
  dfo2->execute_command("start", start_data);
  dfo3->execute_command("start", start_data);
  df1->execute_command("start", start_data);
  df2->execute_command("start", start_data);
  df3->execute_command("start", start_data);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Send a series of trigger decisions
  for (dfmessages::trigger_number_t trig = 1; trig <= 9; ++trig) {
    send_trigdec_to_all_dfos(trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Check that decisions were distributed across DF modules
  size_t df1_count = df1_collector.count_decisions();
  size_t df2_count = df2_collector.count_decisions();
  size_t df3_count = df3_collector.count_decisions();

  TLOG() << "DF1 received " << df1_count << " decisions";
  TLOG() << "DF2 received " << df2_count << " decisions";
  TLOG() << "DF3 received " << df3_count << " decisions";

  // Each DF module should have received some decisions
  BOOST_REQUIRE(df1_count > 0);
  BOOST_REQUIRE(df2_count > 0);
  BOOST_REQUIRE(df3_count > 0);

  // Total should be 9 * 3 (each DFO sends to one DF, but the same DF for each trigger)
  // Actually, each trigger is sent to 3 DFOs, but stable algorithm means all DFOs pick the same DF
  // So total decisions received should be 9 (one per trigger)
  BOOST_REQUIRE_EQUAL(df1_count + df2_count + df3_count, 9);

  // Verify stable algorithm: for each trigger, all DFOs chose the same DF module
  auto df1_decisions = df1_collector.get_decisions();
  auto df2_decisions = df2_collector.get_decisions();
  auto df3_decisions = df3_collector.get_decisions();

  std::set<dfmessages::trigger_number_t> df1_triggers;
  std::set<dfmessages::trigger_number_t> df2_triggers;
  std::set<dfmessages::trigger_number_t> df3_triggers;

  for (const auto& dec : df1_decisions)
    df1_triggers.insert(dec.trigger_number);
  for (const auto& dec : df2_decisions)
    df2_triggers.insert(dec.trigger_number);
  for (const auto& dec : df3_decisions)
    df3_triggers.insert(dec.trigger_number);

  // No trigger should appear in multiple DF modules
  for (const auto& trig : df1_triggers) {
    BOOST_REQUIRE(df2_triggers.find(trig) == df2_triggers.end());
    BOOST_REQUIRE(df3_triggers.find(trig) == df3_triggers.end());
  }
  for (const auto& trig : df2_triggers) {
    BOOST_REQUIRE(df3_triggers.find(trig) == df3_triggers.end());
  }

  collect_opmon_entries();
  auto dfo_infos = get_dfo_info();
  auto dfs_infos = get_dfs_info();
  auto dfo1_metrics = dfo_infos["dfo1"];
  auto dfo2_metrics = dfo_infos["dfo2"];
  auto dfo3_metrics = dfo_infos["dfo3"];
  auto df1_metrics = dfs_infos["df1"];
  auto df2_metrics = dfs_infos["df2"];
  auto df3_metrics = dfs_infos["df3"];

  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_received(), 9);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_received(), 9);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_received(), 9);
  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_sent(), 9);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_sent(), 9);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_sent(), 9);
  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_completed(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_completed(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_completed(), 0);

  BOOST_REQUIRE_EQUAL(df1_metrics.decisions_received(), df1_triggers.size());
  BOOST_REQUIRE_EQUAL(df2_metrics.decisions_received(), df2_triggers.size());
  BOOST_REQUIRE_EQUAL(df3_metrics.decisions_received(), df3_triggers.size());
  BOOST_REQUIRE_EQUAL(df1_metrics.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(df1_metrics.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(df1_metrics.requests_received(), 9 * 3);
  BOOST_REQUIRE_EQUAL(df2_metrics.requests_received(), 9 * 3);
  BOOST_REQUIRE_EQUAL(df3_metrics.requests_received(), 9 * 3);
  BOOST_REQUIRE(df1_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE(df2_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE(df3_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE_EQUAL(df1_metrics.decisions_sent(), df1_triggers.size());
  BOOST_REQUIRE_EQUAL(df2_metrics.decisions_sent(), df2_triggers.size());
  BOOST_REQUIRE_EQUAL(df3_metrics.decisions_sent(), df3_triggers.size());
  BOOST_REQUIRE_EQUAL(df1_metrics.duplicate_decisions_received(), df1_triggers.size() * 2);
  BOOST_REQUIRE_EQUAL(df2_metrics.duplicate_decisions_received(), df2_triggers.size() * 2);
  BOOST_REQUIRE_EQUAL(df3_metrics.duplicate_decisions_received(), df3_triggers.size() * 2);

  // Complete all triggers
  for (const auto& trig : df1_triggers) {
    send_trb_completion("df1", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    send_token("df1", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  for (const auto& trig : df2_triggers) {
    send_trb_completion("df2", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    send_token("df2", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  for (const auto& trig : df3_triggers) {
    send_trb_completion("df3", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    send_token("df3", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  collect_opmon_entries();
  dfo_infos = get_dfo_info();
  dfs_infos = get_dfs_info();
  dfo1_metrics = dfo_infos["dfo1"];
  dfo2_metrics = dfo_infos["dfo2"];
  dfo3_metrics = dfo_infos["dfo3"];
  df1_metrics = dfs_infos["df1"];
  df2_metrics = dfs_infos["df2"];
  df3_metrics = dfs_infos["df3"];

  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_completed(), 9);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_completed(), 9);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_completed(), 9);

  BOOST_REQUIRE_EQUAL(df1_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df1_metrics.tokens_received(), df1_triggers.size());
  BOOST_REQUIRE_EQUAL(df2_metrics.tokens_received(), df2_triggers.size());
  BOOST_REQUIRE_EQUAL(df3_metrics.tokens_received(), df3_triggers.size());
  BOOST_REQUIRE_EQUAL(df1_metrics.trb_completions_received(), df1_triggers.size());
  BOOST_REQUIRE_EQUAL(df2_metrics.trb_completions_received(), df2_triggers.size());
  BOOST_REQUIRE_EQUAL(df3_metrics.trb_completions_received(), df3_triggers.size());
  BOOST_REQUIRE_EQUAL(df1_metrics.requests_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.requests_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.requests_received(), 0);
  BOOST_REQUIRE(df1_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE(df2_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE(df3_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE_EQUAL(df1_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(df1_metrics.duplicate_decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.duplicate_decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.duplicate_decisions_received(), 0);

  // Stop all modules
  dfo1->execute_command("drain_dataflow", null_data);
  dfo2->execute_command("drain_dataflow", null_data);
  dfo3->execute_command("drain_dataflow", null_data);
  df1->execute_command("stop", null_data);
  df2->execute_command("stop", null_data);
  df3->execute_command("stop", null_data);

  dfo1->execute_command("scrap", null_data);
  dfo2->execute_command("scrap", null_data);
  dfo3->execute_command("scrap", null_data);
  df1->execute_command("scrap", null_data);
  df2->execute_command("scrap", null_data);
  df3->execute_command("scrap", null_data);

  df1_recv->remove_callback();
  df2_recv->remove_callback();
  df3_recv->remove_callback();
  dfo1_inh_recv->remove_callback();
  dfo2_inh_recv->remove_callback();
  dfo3_inh_recv->remove_callback();

  TLOG() << "Test case StableAlgorithmOptimalConditions END";
}

BOOST_AUTO_TEST_CASE(AllDFOsAgreeOnAssignments)
{
  TLOG() << "Test case AllDFOsAgreeOnAssignments BEGIN";

  // Create modules
  auto dfo1 = appfwk::make_module("DFOModule", "dfo1");
  auto dfo2 = appfwk::make_module("DFOModule", "dfo2");
  auto dfo3 = appfwk::make_module("DFOModule", "dfo3");
  auto df1 = appfwk::make_module("DataflowStatusModule", "df1");
  auto df2 = appfwk::make_module("DataflowStatusModule", "df2");
  auto df3 = appfwk::make_module("DataflowStatusModule", "df3");

  opmgr.register_node("dfo1", dfo1);
  opmgr.register_node("dfo2", dfo2);
  opmgr.register_node("dfo3", dfo3);
  opmgr.register_node("df1", df1);
  opmgr.register_node("df2", df2);
  opmgr.register_node("df3", df3);

  dfo1->init(cfgMgr);
  dfo2->init(cfgMgr);
  dfo3->init(cfgMgr);
  df1->init(cfgMgr);
  df2->init(cfgMgr);
  df3->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  dfo1->execute_command("conf", null_data);
  dfo2->execute_command("conf", null_data);
  dfo3->execute_command("conf", null_data);
  df1->execute_command("conf", null_data);
  df2->execute_command("conf", null_data);
  df3->execute_command("conf", null_data);

  auto iom = iomanager::IOManager::get();
  df1_collector.reset();
  df2_collector.reset();
  df3_collector.reset();
  dfo_collector.reset();

  auto df1_recv = iom->get_receiver<dfmessages::TriggerDecision>("df1_trigdec_out");
  df1_recv->add_callback(df1_recv_trigdec);
  auto df2_recv = iom->get_receiver<dfmessages::TriggerDecision>("df2_trigdec_out");
  df2_recv->add_callback(df2_recv_trigdec);
  auto df3_recv = iom->get_receiver<dfmessages::TriggerDecision>("df3_trigdec_out");
  df3_recv->add_callback(df3_recv_trigdec);

  auto dfo1_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo1_triginh_out");
  dfo1_inh_recv->add_callback(dfo1_recv_inhibit);
  auto dfo2_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo2_triginh_out");
  dfo2_inh_recv->add_callback(dfo2_recv_inhibit);
  auto dfo3_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo3_triginh_out");
  dfo3_inh_recv->add_callback(dfo3_recv_inhibit);

  dfo1->execute_command("start", start_data);
  dfo2->execute_command("start", start_data);
  dfo3->execute_command("start", start_data);
  df1->execute_command("start", start_data);
  df2->execute_command("start", start_data);
  df3->execute_command("start", start_data);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Send trigger decisions
  for (dfmessages::trigger_number_t trig = 10; trig <= 15; ++trig) {
    send_trigdec_to_all_dfos(trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Verify each trigger was received by exactly one DF module
  auto df1_decisions = df1_collector.get_decisions();
  auto df2_decisions = df2_collector.get_decisions();
  auto df3_decisions = df3_collector.get_decisions();

  std::map<dfmessages::trigger_number_t, int> trigger_counts;
  for (const auto& dec : df1_decisions)
    trigger_counts[dec.trigger_number]++;
  for (const auto& dec : df2_decisions)
    trigger_counts[dec.trigger_number]++;
  for (const auto& dec : df3_decisions)
    trigger_counts[dec.trigger_number]++;

  // Each trigger should have been received exactly once
  for (dfmessages::trigger_number_t trig = 10; trig <= 15; ++trig) {
    BOOST_REQUIRE_EQUAL(trigger_counts[trig], 1);
  }

  // Check that all DFOs have matching inhibit states
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  bool dfo1_busy = dfo_collector.get_latest_inhibit("dfo1");
  bool dfo2_busy = dfo_collector.get_latest_inhibit("dfo2");
  bool dfo3_busy = dfo_collector.get_latest_inhibit("dfo3");

  TLOG() << "DFO1 inhibit state: " << std::boolalpha << dfo1_busy;
  TLOG() << "DFO2 inhibit state: " << std::boolalpha << dfo2_busy;
  TLOG() << "DFO3 inhibit state: " << std::boolalpha << dfo3_busy;

  // All DFOs should agree on busy state
  BOOST_REQUIRE_EQUAL(dfo1_busy, dfo2_busy);
  BOOST_REQUIRE_EQUAL(dfo2_busy, dfo3_busy);

  // Complete triggers
  std::set<dfmessages::trigger_number_t> df1_triggers;
  std::set<dfmessages::trigger_number_t> df2_triggers;
  std::set<dfmessages::trigger_number_t> df3_triggers;

  for (const auto& dec : df1_decisions)
    df1_triggers.insert(dec.trigger_number);
  for (const auto& dec : df2_decisions)
    df2_triggers.insert(dec.trigger_number);
  for (const auto& dec : df3_decisions)
    df3_triggers.insert(dec.trigger_number);

  collect_opmon_entries();
  auto dfo_infos = get_dfo_info();
  auto dfs_infos = get_dfs_info();
  auto dfo1_metrics = dfo_infos["dfo1"];
  auto dfo2_metrics = dfo_infos["dfo2"];
  auto dfo3_metrics = dfo_infos["dfo3"];
  auto df1_metrics = dfs_infos["df1"];
  auto df2_metrics = dfs_infos["df2"];
  auto df3_metrics = dfs_infos["df3"];

  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_received(), 6);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_received(), 6);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_received(), 6);
  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_sent(), 6);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_sent(), 6);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_sent(), 6);
  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_completed(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_completed(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_completed(), 0);

  BOOST_REQUIRE_EQUAL(df1_metrics.decisions_received(), df1_triggers.size());
  BOOST_REQUIRE_EQUAL(df2_metrics.decisions_received(), df2_triggers.size());
  BOOST_REQUIRE_EQUAL(df3_metrics.decisions_received(), df3_triggers.size());
  BOOST_REQUIRE_EQUAL(df1_metrics.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(df1_metrics.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(df1_metrics.requests_received(), 6 * 3);
  BOOST_REQUIRE_EQUAL(df2_metrics.requests_received(), 6 * 3);
  BOOST_REQUIRE_EQUAL(df3_metrics.requests_received(), 6 * 3);
  BOOST_REQUIRE(df1_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE(df2_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE(df3_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE_EQUAL(df1_metrics.decisions_sent(), df1_triggers.size());
  BOOST_REQUIRE_EQUAL(df2_metrics.decisions_sent(), df2_triggers.size());
  BOOST_REQUIRE_EQUAL(df3_metrics.decisions_sent(), df3_triggers.size());
  BOOST_REQUIRE_EQUAL(df1_metrics.duplicate_decisions_received(), df1_triggers.size() * 2);
  BOOST_REQUIRE_EQUAL(df2_metrics.duplicate_decisions_received(), df2_triggers.size() * 2);
  BOOST_REQUIRE_EQUAL(df3_metrics.duplicate_decisions_received(), df3_triggers.size() * 2);


  for (const auto& trig : df1_triggers) {
    send_trb_completion("df1", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    send_token("df1", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  for (const auto& trig : df2_triggers) {
    send_trb_completion("df2", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    send_token("df2", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  for (const auto& trig : df3_triggers) {
    send_trb_completion("df3", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    send_token("df3", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  collect_opmon_entries();
  dfo_infos = get_dfo_info();
  dfs_infos = get_dfs_info();
  dfo1_metrics = dfo_infos["dfo1"];
  dfo2_metrics = dfo_infos["dfo2"];
  dfo3_metrics = dfo_infos["dfo3"];
  df1_metrics = dfs_infos["df1"];
  df2_metrics = dfs_infos["df2"];
  df3_metrics = dfs_infos["df3"];

  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_completed(), 6);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_completed(), 6);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_completed(), 6);

  BOOST_REQUIRE_EQUAL(df1_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df1_metrics.tokens_received(), df1_triggers.size());
  BOOST_REQUIRE_EQUAL(df2_metrics.tokens_received(), df2_triggers.size());
  BOOST_REQUIRE_EQUAL(df3_metrics.tokens_received(), df3_triggers.size());
  BOOST_REQUIRE_EQUAL(df1_metrics.trb_completions_received(), df1_triggers.size());
  BOOST_REQUIRE_EQUAL(df2_metrics.trb_completions_received(), df2_triggers.size());
  BOOST_REQUIRE_EQUAL(df3_metrics.trb_completions_received(), df3_triggers.size());
  BOOST_REQUIRE_EQUAL(df1_metrics.requests_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.requests_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.requests_received(), 0);
  BOOST_REQUIRE(df1_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE(df2_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE(df3_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE_EQUAL(df1_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(df1_metrics.duplicate_decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.duplicate_decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.duplicate_decisions_received(), 0);

  dfo1->execute_command("drain_dataflow", null_data);
  dfo2->execute_command("drain_dataflow", null_data);
  dfo3->execute_command("drain_dataflow", null_data);
  df1->execute_command("stop", null_data);
  df2->execute_command("stop", null_data);
  df3->execute_command("stop", null_data);

  dfo1->execute_command("scrap", null_data);
  dfo2->execute_command("scrap", null_data);
  dfo3->execute_command("scrap", null_data);
  df1->execute_command("scrap", null_data);
  df2->execute_command("scrap", null_data);
  df3->execute_command("scrap", null_data);

  df1_recv->remove_callback();
  df2_recv->remove_callback();
  df3_recv->remove_callback();
  dfo1_inh_recv->remove_callback();
  dfo2_inh_recv->remove_callback();
  dfo3_inh_recv->remove_callback();

  TLOG() << "Test case AllDFOsAgreeOnAssignments END";
}

BOOST_AUTO_TEST_CASE(DelayedDFOStillMatchesAssignments)
{
  TLOG() << "Test case DelayedDFOStillMatchesAssignments BEGIN";

  // Create modules
  auto dfo1 = appfwk::make_module("DFOModule", "dfo1");
  auto dfo2 = appfwk::make_module("DFOModule", "dfo2");
  auto dfo3 = appfwk::make_module("DFOModule", "dfo3");
  auto df1 = appfwk::make_module("DataflowStatusModule", "df1");
  auto df2 = appfwk::make_module("DataflowStatusModule", "df2");
  auto df3 = appfwk::make_module("DataflowStatusModule", "df3");

  opmgr.register_node("dfo1", dfo1);
  opmgr.register_node("dfo2", dfo2);
  opmgr.register_node("dfo3", dfo3);
  opmgr.register_node("df1", df1);
  opmgr.register_node("df2", df2);
  opmgr.register_node("df3", df3);

  dfo1->init(cfgMgr);
  dfo2->init(cfgMgr);
  dfo3->init(cfgMgr);
  df1->init(cfgMgr);
  df2->init(cfgMgr);
  df3->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  dfo1->execute_command("conf", null_data);
  dfo2->execute_command("conf", null_data);
  dfo3->execute_command("conf", null_data);
  df1->execute_command("conf", null_data);
  df2->execute_command("conf", null_data);
  df3->execute_command("conf", null_data);

  auto iom = iomanager::IOManager::get();
  df1_collector.reset();
  df2_collector.reset();
  df3_collector.reset();
  dfo_collector.reset();

  auto df1_recv = iom->get_receiver<dfmessages::TriggerDecision>("df1_trigdec_out");
  df1_recv->add_callback(df1_recv_trigdec);
  auto df2_recv = iom->get_receiver<dfmessages::TriggerDecision>("df2_trigdec_out");
  df2_recv->add_callback(df2_recv_trigdec);
  auto df3_recv = iom->get_receiver<dfmessages::TriggerDecision>("df3_trigdec_out");
  df3_recv->add_callback(df3_recv_trigdec);

  auto dfo1_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo1_triginh_out");
  dfo1_inh_recv->add_callback(dfo1_recv_inhibit);
  auto dfo2_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo2_triginh_out");
  dfo2_inh_recv->add_callback(dfo2_recv_inhibit);
  auto dfo3_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo3_triginh_out");
  dfo3_inh_recv->add_callback(dfo3_recv_inhibit);

  // Start DFO1 and DFO2 first, delay DFO3
  dfo1->execute_command("start", start_data);
  dfo2->execute_command("start", start_data);
  df1->execute_command("start", start_data);
  df2->execute_command("start", start_data);
  df3->execute_command("start", start_data);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Send triggers to DFO1 and DFO2 only initially
  auto send_to_dfo1_dfo2 = [&iom](dfmessages::trigger_number_t trigger_number) {
    for (const auto& conn : { "dfo1_trigdec_in", "dfo2_trigdec_in" }) {
      dfmessages::TriggerDecision td;
      td.trigger_number = trigger_number;
      td.run_number = 1;
      td.trigger_timestamp = trigger_number * 1000;
      td.trigger_type = 1;
      td.readout_type = dfmessages::ReadoutType::kLocalized;

      auto sender = iom->get_sender<dfmessages::TriggerDecision>(conn);
      sender->send(std::move(td), iomanager::Sender::s_block);
    }
  };

  for (dfmessages::trigger_number_t trig = 20; trig <= 25; ++trig) {
    send_to_dfo1_dfo2(trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Now start DFO3 (delayed)
  dfo3->execute_command("start", start_data);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Send same triggers to DFO3
  for (dfmessages::trigger_number_t trig = 20; trig <= 25; ++trig) {
    dfmessages::TriggerDecision td;
    td.trigger_number = trig;
    td.run_number = 1;
    td.trigger_timestamp = trig * 1000;
    td.trigger_type = 1;
    td.readout_type = dfmessages::ReadoutType::kLocalized;

    auto sender = iom->get_sender<dfmessages::TriggerDecision>("dfo3_trigdec_in");
    sender->send(std::move(td), iomanager::Sender::s_block);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Verify that each trigger was still assigned to exactly one DF module
  auto df1_decisions = df1_collector.get_decisions();
  auto df2_decisions = df2_collector.get_decisions();
  auto df3_decisions = df3_collector.get_decisions();

  std::map<dfmessages::trigger_number_t, int> trigger_counts;
  for (const auto& dec : df1_decisions)
    trigger_counts[dec.trigger_number]++;
  for (const auto& dec : df2_decisions)
    trigger_counts[dec.trigger_number]++;
  for (const auto& dec : df3_decisions)
    trigger_counts[dec.trigger_number]++;

  // Each trigger should have been received exactly once despite delayed DFO3
  for (dfmessages::trigger_number_t trig = 20; trig <= 25; ++trig) {
    TLOG() << "Trigger " << trig << " received " << trigger_counts[trig] << " times";
    BOOST_REQUIRE_EQUAL(trigger_counts[trig], 1);
  }

  // Complete triggers
  std::set<dfmessages::trigger_number_t> df1_triggers;
  std::set<dfmessages::trigger_number_t> df2_triggers;
  std::set<dfmessages::trigger_number_t> df3_triggers;

  for (const auto& dec : df1_decisions)
    df1_triggers.insert(dec.trigger_number);
  for (const auto& dec : df2_decisions)
    df2_triggers.insert(dec.trigger_number);
  for (const auto& dec : df3_decisions)
    df3_triggers.insert(dec.trigger_number);

  collect_opmon_entries();
  auto dfo_infos = get_dfo_info();
  auto dfs_infos = get_dfs_info();
  auto dfo1_metrics = dfo_infos["dfo1"];
  auto dfo2_metrics = dfo_infos["dfo2"];
  auto dfo3_metrics = dfo_infos["dfo3"];
  auto df1_metrics = dfs_infos["df1"];
  auto df2_metrics = dfs_infos["df2"];
  auto df3_metrics = dfs_infos["df3"];

  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_received(), 6);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_received(), 6);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_received(), 6);
  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_sent(), 6);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_sent(), 6);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_sent(), 6);
  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_completed(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_completed(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_completed(), 0);

  BOOST_REQUIRE_EQUAL(df1_metrics.decisions_received(), df1_triggers.size());
  BOOST_REQUIRE_EQUAL(df2_metrics.decisions_received(), df2_triggers.size());
  BOOST_REQUIRE_EQUAL(df3_metrics.decisions_received(), df3_triggers.size());
  BOOST_REQUIRE_EQUAL(df1_metrics.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(df1_metrics.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.trb_completions_received(), 0);
  BOOST_REQUIRE_EQUAL(df1_metrics.requests_received(), 6 * 3);
  BOOST_REQUIRE_EQUAL(df2_metrics.requests_received(), 6 * 3);
  BOOST_REQUIRE_EQUAL(df3_metrics.requests_received(), 6 * 3);
  BOOST_REQUIRE(df1_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE(df2_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE(df3_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE_EQUAL(df1_metrics.decisions_sent(), df1_triggers.size());
  BOOST_REQUIRE_EQUAL(df2_metrics.decisions_sent(), df2_triggers.size());
  BOOST_REQUIRE_EQUAL(df3_metrics.decisions_sent(), df3_triggers.size());
  BOOST_REQUIRE_EQUAL(df1_metrics.duplicate_decisions_received(), df1_triggers.size() * 2);
  BOOST_REQUIRE_EQUAL(df2_metrics.duplicate_decisions_received(), df2_triggers.size() * 2);
  BOOST_REQUIRE_EQUAL(df3_metrics.duplicate_decisions_received(), df3_triggers.size() * 2);

  for (const auto& trig : df1_triggers) {
    send_trb_completion("df1", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    send_token("df1", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  for (const auto& trig : df2_triggers) {
    send_trb_completion("df2", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    send_token("df2", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  for (const auto& trig : df3_triggers) {
    send_trb_completion("df3", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    send_token("df3", trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  collect_opmon_entries();
  dfo_infos = get_dfo_info();
  dfs_infos = get_dfs_info();
  dfo1_metrics = dfo_infos["dfo1"];
  dfo2_metrics = dfo_infos["dfo2"];
  dfo3_metrics = dfo_infos["dfo3"];
  df1_metrics = dfs_infos["df1"];
  df2_metrics = dfs_infos["df2"];
  df3_metrics = dfs_infos["df3"];

  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(dfo1_metrics.decisions_completed(), 6);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_completed(), 6);
  BOOST_REQUIRE_EQUAL(dfo2_metrics.decisions_completed(), 6);

  BOOST_REQUIRE_EQUAL(df1_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df1_metrics.tokens_received(), df1_triggers.size());
  BOOST_REQUIRE_EQUAL(df2_metrics.tokens_received(), df2_triggers.size());
  BOOST_REQUIRE_EQUAL(df3_metrics.tokens_received(), df3_triggers.size());
  BOOST_REQUIRE_EQUAL(df1_metrics.trb_completions_received(), df1_triggers.size());
  BOOST_REQUIRE_EQUAL(df2_metrics.trb_completions_received(), df2_triggers.size());
  BOOST_REQUIRE_EQUAL(df3_metrics.trb_completions_received(), df3_triggers.size());
  BOOST_REQUIRE_EQUAL(df1_metrics.requests_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.requests_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.requests_received(), 0);
  BOOST_REQUIRE(df1_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE(df2_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE(df3_metrics.status_messages_sent() >= 1);
  BOOST_REQUIRE_EQUAL(df1_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(df1_metrics.duplicate_decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df2_metrics.duplicate_decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(df3_metrics.duplicate_decisions_received(), 0);

  dfo1->execute_command("drain_dataflow", null_data);
  dfo2->execute_command("drain_dataflow", null_data);
  dfo3->execute_command("drain_dataflow", null_data);
  df1->execute_command("stop", null_data);
  df2->execute_command("stop", null_data);
  df3->execute_command("stop", null_data);

  dfo1->execute_command("scrap", null_data);
  dfo2->execute_command("scrap", null_data);
  dfo3->execute_command("scrap", null_data);
  df1->execute_command("scrap", null_data);
  df2->execute_command("scrap", null_data);
  df3->execute_command("scrap", null_data);

  df1_recv->remove_callback();
  df2_recv->remove_callback();
  df3_recv->remove_callback();
  dfo1_inh_recv->remove_callback();
  dfo2_inh_recv->remove_callback();
  dfo3_inh_recv->remove_callback();

  TLOG() << "Test case DelayedDFOStillMatchesAssignments END";
}

BOOST_AUTO_TEST_CASE(DFOCrashRecovery)
{
  TLOG() << "Test case DFOCrashRecovery BEGIN";

  // Create modules
  auto dfo1 = appfwk::make_module("DFOModule", "dfo1");
  auto dfo2 = appfwk::make_module("DFOModule", "dfo2");
  auto dfo3 = appfwk::make_module("DFOModule", "dfo3");
  auto df1 = appfwk::make_module("DataflowStatusModule", "df1");
  auto df2 = appfwk::make_module("DataflowStatusModule", "df2");
  auto df3 = appfwk::make_module("DataflowStatusModule", "df3");

  opmgr.register_node("dfo1", dfo1);
  opmgr.register_node("dfo2", dfo2);
  opmgr.register_node("dfo3", dfo3);
  opmgr.register_node("df1", df1);
  opmgr.register_node("df2", df2);
  opmgr.register_node("df3", df3);

  dfo1->init(cfgMgr);
  dfo2->init(cfgMgr);
  dfo3->init(cfgMgr);
  df1->init(cfgMgr);
  df2->init(cfgMgr);
  df3->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  dfo1->execute_command("conf", null_data);
  dfo2->execute_command("conf", null_data);
  dfo3->execute_command("conf", null_data);
  df1->execute_command("conf", null_data);
  df2->execute_command("conf", null_data);
  df3->execute_command("conf", null_data);

  auto iom = iomanager::IOManager::get();
  df1_collector.reset();
  df2_collector.reset();
  df3_collector.reset();

  auto df1_recv = iom->get_receiver<dfmessages::TriggerDecision>("df1_trigdec_out");
  df1_recv->add_callback(df1_recv_trigdec);
  auto df2_recv = iom->get_receiver<dfmessages::TriggerDecision>("df2_trigdec_out");
  df2_recv->add_callback(df2_recv_trigdec);
  auto df3_recv = iom->get_receiver<dfmessages::TriggerDecision>("df3_trigdec_out");
  df3_recv->add_callback(df3_recv_trigdec);

  auto dfo1_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo1_triginh_out");
  dfo1_inh_recv->add_callback(dfo1_recv_inhibit);
  auto dfo2_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo2_triginh_out");
  dfo2_inh_recv->add_callback(dfo2_recv_inhibit);
  auto dfo3_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo3_triginh_out");
  dfo3_inh_recv->add_callback(dfo3_recv_inhibit);

  // Start all modules
  dfo1->execute_command("start", start_data);
  dfo2->execute_command("start", start_data);
  dfo3->execute_command("start", start_data);
  df1->execute_command("start", start_data);
  df2->execute_command("start", start_data);
  df3->execute_command("start", start_data);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Send a few triggers
  for (dfmessages::trigger_number_t trig = 30; trig <= 32; ++trig) {
    send_trigdec_to_all_dfos(trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Simulate DFO2 crash by stopping it abruptly
  TLOG() << "Simulating DFO2 crash...";
  dfo2->execute_command("scrap", null_data);

  // Continue sending triggers to remaining DFOs
  for (dfmessages::trigger_number_t trig = 33; trig <= 35; ++trig) {
    // Send only to DFO1 and DFO3
    for (const auto& conn : { "dfo1_trigdec_in", "dfo3_trigdec_in" }) {
      dfmessages::TriggerDecision td;
      td.trigger_number = trig;
      td.run_number = 1;
      td.trigger_timestamp = trig * 1000;
      td.trigger_type = 1;
      td.readout_type = dfmessages::ReadoutType::kLocalized;

      auto sender = iom->get_sender<dfmessages::TriggerDecision>(conn);
      sender->send(std::move(td), iomanager::Sender::s_block);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // System should continue working with remaining DFOs
  auto df1_decisions = df1_collector.get_decisions();
  auto df2_decisions = df2_collector.get_decisions();
  auto df3_decisions = df3_collector.get_decisions();

  size_t total_decisions = df1_decisions.size() + df2_decisions.size() + df3_decisions.size();
  TLOG() << "Total decisions received: " << total_decisions;

  // Should have received 6 decisions total (triggers 30-35)
  BOOST_REQUIRE_EQUAL(total_decisions, 6);

  // Complete triggers
  std::set<dfmessages::trigger_number_t> df1_triggers;
  std::set<dfmessages::trigger_number_t> df2_triggers;
  std::set<dfmessages::trigger_number_t> df3_triggers;

  for (const auto& dec : df1_decisions)
    df1_triggers.insert(dec.trigger_number);
  for (const auto& dec : df2_decisions)
    df2_triggers.insert(dec.trigger_number);
  for (const auto& dec : df3_decisions)
    df3_triggers.insert(dec.trigger_number);

  for (const auto& trig : df1_triggers) {
    send_trb_completion("df1", trig);
    send_token("df1", trig);
  }
  for (const auto& trig : df2_triggers) {
    send_trb_completion("df2", trig);
    send_token("df2", trig);
  }
  for (const auto& trig : df3_triggers) {
    send_trb_completion("df3", trig);
    send_token("df3", trig);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  dfo1->execute_command("drain_dataflow", null_data);
  dfo3->execute_command("drain_dataflow", null_data);
  df1->execute_command("stop", null_data);
  df2->execute_command("stop", null_data);
  df3->execute_command("stop", null_data);

  dfo1->execute_command("scrap", null_data);
  dfo3->execute_command("scrap", null_data);
  df1->execute_command("scrap", null_data);
  df2->execute_command("scrap", null_data);
  df3->execute_command("scrap", null_data);

  df1_recv->remove_callback();
  df2_recv->remove_callback();
  df3_recv->remove_callback();
  dfo1_inh_recv->remove_callback();
  dfo2_inh_recv->remove_callback();
  dfo3_inh_recv->remove_callback();

  TLOG() << "Test case DFOCrashRecovery END";
}

BOOST_AUTO_TEST_CASE(DataflowStatusModuleCrashAndRecovery)
{
  TLOG() << "Test case DataflowStatusModuleCrashAndRecovery BEGIN";

  // Create modules
  auto dfo1 = appfwk::make_module("DFOModule", "dfo1");
  auto dfo2 = appfwk::make_module("DFOModule", "dfo2");
  auto dfo3 = appfwk::make_module("DFOModule", "dfo3");
  auto df1 = appfwk::make_module("DataflowStatusModule", "df1");
  auto df2 = appfwk::make_module("DataflowStatusModule", "df2");
  auto df3 = appfwk::make_module("DataflowStatusModule", "df3");

  opmgr.register_node("dfo1", dfo1);
  opmgr.register_node("dfo2", dfo2);
  opmgr.register_node("dfo3", dfo3);
  opmgr.register_node("df1", df1);
  opmgr.register_node("df2", df2);
  opmgr.register_node("df3", df3);

  dfo1->init(cfgMgr);
  dfo2->init(cfgMgr);
  dfo3->init(cfgMgr);
  df1->init(cfgMgr);
  df2->init(cfgMgr);
  df3->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  dfo1->execute_command("conf", null_data);
  dfo2->execute_command("conf", null_data);
  dfo3->execute_command("conf", null_data);
  df1->execute_command("conf", null_data);
  df2->execute_command("conf", null_data);
  df3->execute_command("conf", null_data);

  auto iom = iomanager::IOManager::get();
  df1_collector.reset();
  df2_collector.reset();
  df3_collector.reset();

  auto df1_recv = iom->get_receiver<dfmessages::TriggerDecision>("df1_trigdec_out");
  df1_recv->add_callback(df1_recv_trigdec);
  auto df2_recv = iom->get_receiver<dfmessages::TriggerDecision>("df2_trigdec_out");
  df2_recv->add_callback(df2_recv_trigdec);
  auto df3_recv = iom->get_receiver<dfmessages::TriggerDecision>("df3_trigdec_out");
  df3_recv->add_callback(df3_recv_trigdec);

  auto dfo1_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo1_triginh_out");
  dfo1_inh_recv->add_callback(dfo1_recv_inhibit);
  auto dfo2_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo2_triginh_out");
  dfo2_inh_recv->add_callback(dfo2_recv_inhibit);
  auto dfo3_inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("dfo3_triginh_out");
  dfo3_inh_recv->add_callback(dfo3_recv_inhibit);

  // Start all modules
  dfo1->execute_command("start", start_data);
  dfo2->execute_command("start", start_data);
  dfo3->execute_command("start", start_data);
  df1->execute_command("start", start_data);
  df2->execute_command("start", start_data);
  df3->execute_command("start", start_data);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Send a few triggers before crash
  for (dfmessages::trigger_number_t trig = 40; trig <= 42; ++trig) {
    send_trigdec_to_all_dfos(trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  auto pre_crash_df1_count = df1_collector.count_decisions();
  auto pre_crash_df2_count = df2_collector.count_decisions();
  auto pre_crash_df3_count = df3_collector.count_decisions();

  TLOG() << "Before crash - DF1: " << pre_crash_df1_count << ", DF2: " << pre_crash_df2_count
         << ", DF3: " << pre_crash_df3_count;

  // Simulate DF2 crash by stopping it
  TLOG() << "Simulating DF2 crash...";
  df2->execute_command("stop", null_data);
  df2->execute_command("scrap", null_data);

  // Wait for DF2 to timeout (5+ seconds)
  TLOG() << "Waiting for DF2 heartbeat timeout...";
  std::this_thread::sleep_for(std::chrono::milliseconds(5500));

  // Send more triggers - they should be redistributed to DF1 and DF3 only
  for (dfmessages::trigger_number_t trig = 43; trig <= 45; ++trig) {
    send_trigdec_to_all_dfos(trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  auto post_crash_df1_count = df1_collector.count_decisions();
  auto post_crash_df2_count = df2_collector.count_decisions();
  auto post_crash_df3_count = df3_collector.count_decisions();

  TLOG() << "After crash - DF1: " << post_crash_df1_count << ", DF2: " << post_crash_df2_count
         << ", DF3: " << post_crash_df3_count;

  // DF2 should not have received any new triggers
  BOOST_REQUIRE_EQUAL(post_crash_df2_count, pre_crash_df2_count);

  // DF1 and DF3 should have received the new triggers, plus triggers from df2
  BOOST_REQUIRE(post_crash_df1_count > pre_crash_df1_count || post_crash_df3_count > pre_crash_df3_count);

  // Total should be 6 (triggers 40-45)
  BOOST_REQUIRE_EQUAL(post_crash_df1_count + post_crash_df3_count, 6);

  // Now simulate DF2 recovery
  TLOG() << "Simulating DF2 recovery...";
  df2 = appfwk::make_module("DataflowStatusModule", "df2");
  opmgr.register_node("df2", df2);
  df2->init(cfgMgr);
  df2->execute_command("conf", null_data);

  df2_collector.reset();
  df2_recv = iom->get_receiver<dfmessages::TriggerDecision>("df2_trigdec_out");
  df2_recv->add_callback(df2_recv_trigdec);

  df2->execute_command("start", start_data);
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Send more triggers - DF2 should start receiving them again
  for (dfmessages::trigger_number_t trig = 46; trig <= 48; ++trig) {
    send_trigdec_to_all_dfos(trig);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  auto recovery_df2_count = df2_collector.count_decisions();
  TLOG() << "After recovery - DF2: " << recovery_df2_count;

  // DF2 should have received at least one trigger after recovery
  BOOST_REQUIRE(recovery_df2_count > 0);

  // Complete all triggers
  auto df1_decisions = df1_collector.get_decisions();
  auto df2_decisions = df2_collector.get_decisions();
  auto df3_decisions = df3_collector.get_decisions();

  std::set<dfmessages::trigger_number_t> df1_triggers;
  std::set<dfmessages::trigger_number_t> df2_triggers;
  std::set<dfmessages::trigger_number_t> df3_triggers;

  for (const auto& dec : df1_decisions)
    df1_triggers.insert(dec.trigger_number);
  for (const auto& dec : df2_decisions)
    df2_triggers.insert(dec.trigger_number);
  for (const auto& dec : df3_decisions)
    df3_triggers.insert(dec.trigger_number);

  for (const auto& trig : df1_triggers) {
    send_trb_completion("df1", trig);
    send_token("df1", trig);
  }
  for (const auto& trig : df2_triggers) {
    send_trb_completion("df2", trig);
    send_token("df2", trig);
  }
  for (const auto& trig : df3_triggers) {
    send_trb_completion("df3", trig);
    send_token("df3", trig);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  dfo1->execute_command("drain_dataflow", null_data);
  dfo2->execute_command("drain_dataflow", null_data);
  dfo3->execute_command("drain_dataflow", null_data);
  df1->execute_command("stop", null_data);
  df2->execute_command("stop", null_data);
  df3->execute_command("stop", null_data);

  dfo1->execute_command("scrap", null_data);
  dfo2->execute_command("scrap", null_data);
  dfo3->execute_command("scrap", null_data);
  df1->execute_command("scrap", null_data);
  df2->execute_command("scrap", null_data);
  df3->execute_command("scrap", null_data);

  df1_recv->remove_callback();
  df2_recv->remove_callback();
  df3_recv->remove_callback();
  dfo1_inh_recv->remove_callback();
  dfo2_inh_recv->remove_callback();
  dfo3_inh_recv->remove_callback();

  TLOG() << "Test case DataflowStatusModuleCrashAndRecovery END";
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace dunedaq
