/**
 * @file DFOModule_test.cxx Test application that tests and demonstrates
 * the functionality of the DFOModule class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DFOModule.hpp"

#include "appmodel/DFOConf.hpp"
#include "appmodel/DFOModule.hpp"
#include "dfmessages/DFODecision.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TriggerInhibit.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/opmon/DFOModule.pb.h"
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
  {
    std::string oksConfig = "oksconflibs:test/config/datafloworchestrator_test.data.xml";
    std::string appName = "TestApp";
    std::string sessionName = "partition_name";
    cfgMgr = std::make_shared<dunedaq::appfwk::ConfigurationManager>(oksConfig, appName, sessionName);
    get_iomanager()->configure(sessionName, cfgMgr->get_queues(), cfgMgr->get_networkconnections(), nullptr, opmgr);
  }
  ~CfgFixture() { get_iomanager()->reset(); }

  auto get_dfo_info()
  {

    opmgr.collect();
    auto opmon_facility = opmgr.get_backend_facility();
    auto list = opmon_facility->get_entries(std::regex(".*DFOInfo"));
    BOOST_REQUIRE_EQUAL(list.size(), 1);
    const auto& entry = list.front();
    return opmonlib::from_entry<dfmodules::opmon::DFOInfo>(entry);
  }

  dunedaq::opmonlib::TestOpMonManager opmgr;
  std::shared_ptr<dunedaq::appfwk::ConfigurationManager> cfgMgr;
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
  dfo->init(cfgMgr);
}

BOOST_AUTO_TEST_CASE(Commands)
{
  auto dfo = appfwk::make_module("DFOModule", "test");
  opmgr.register_node("dfo", dfo);
  dfo->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  dfo->execute_command("conf", null_data);
  dfo->execute_command("start", start_data);
  dfo->execute_command("drain_dataflow", null_data);
  dfo->execute_command("scrap", null_data);

  auto metric = get_dfo_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.forwarding_decision(), 0);
  BOOST_REQUIRE_EQUAL(metric.waiting_for_decision(), 0);
  BOOST_REQUIRE_EQUAL(metric.deciding_destination(), 0);
  BOOST_REQUIRE_EQUAL(metric.waiting_for_token(), 0);
  BOOST_REQUIRE_EQUAL(metric.processing_token(), 0);
}

BOOST_AUTO_TEST_CASE(DataFlow)
{
  auto dfo = appfwk::make_module("DFOModule", "test");
  opmgr.register_node("dfo", dfo);
  dfo->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  dfo->execute_command("conf", null_data);

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

  // Note: Counters are reset by calling get_dfo_info!
  auto metric = get_dfo_info();

  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);

  dfo->execute_command("start", start_data);
  send_init_token();

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  metric = get_dfo_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);

  send_trigdec(2);
  send_trigdec(3);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  send_trigdec(4);

  metric = get_dfo_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 2);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 2);

  BOOST_REQUIRE(busy_signal_recvd.load());
  std::this_thread::sleep_for(std::chrono::milliseconds(400));

  metric = get_dfo_info();
  BOOST_REQUIRE_EQUAL(metric.tokens_received(), 3);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 1);
  BOOST_REQUIRE(!busy_signal_recvd.load());

  dfo->execute_command("drain_dataflow", null_data);
  dfo->execute_command("scrap", null_data);

  dec_recv->remove_callback();
  inh_recv->remove_callback();
}

BOOST_AUTO_TEST_CASE(SendTrigDecFailed)
{
  auto dfo = appfwk::make_module("DFOModule", "test");
  opmgr.register_node("dfo", dfo);
  dfo->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  dfo->execute_command("conf", null_data);

  auto iom = iomanager::IOManager::get();
  auto inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("triginh");
  inh_recv->add_callback([](const dfmessages::TriggerInhibit&) {});

  dfo->execute_command("start", start_data);

  send_init_token("invalid_connection");

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  send_trigdec(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  auto info = get_dfo_info();
  BOOST_REQUIRE_EQUAL(info.tokens_received(), 0);
  BOOST_REQUIRE_EQUAL(info.decisions_received(), 1);
  BOOST_REQUIRE_EQUAL(info.decisions_sent(), 0);

  // FWIW, tell the DFO to retry the invalid connection
  send_token(1000, "invalid_connection");
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Token for unknown dataflow app
  send_token(1000);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  BOOST_TEST_MESSAGE("Draining dataflow and scrapping DFO");
  dfo->execute_command("drain_dataflow", null_data);
  dfo->execute_command("scrap", null_data);

  inh_recv->remove_callback();
}

// ---------------------------------------------------------------------------
// Peer-announcement magic value
//    Verify that s_peer_announce_magic does not equal 0 and is max for the type.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(PeerAnnounceMagicValue)
{
  BOOST_REQUIRE_NE(DFOModule::s_peer_announce_magic, static_cast<daqdataformats::trigger_number_t>(0));
  BOOST_REQUIRE_EQUAL(DFOModule::s_peer_announce_magic, std::numeric_limits<daqdataformats::trigger_number_t>::max());
}

// ---------------------------------------------------------------------------
// Partition-filter logic (unit test without networking)
//    Instantiate the DFOModule and manually exercise the filter by
//    sending trigger decisions at various trigger_numbers. With two DFOs
//    (indices 0 and 1), decisions with even trigger_numbers go to the DFO
//    at index 0 and odd ones to index 1.
//    Here we simulate the DFO that has own_index=0 and num_dfos=2 by
//    starting it and injecting a synthetic peer announcement so the partition
//    settles before the first trigger decision arrives.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(PartitionFilter)
{
  auto dfo = appfwk::make_module("DFOModule", "test_consensus");
  opmgr.register_node("dfo", dfo);
  dfo->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  dfo->execute_command("conf", null_data);

  auto iom = iomanager::IOManager::get();

  // Count how many trigger decisions actually reach the TRB.
  std::atomic<uint64_t> received_count{ 0 };
  auto dec_recv = iom->get_receiver<dfmessages::TriggerDecision>("trigdec_0");
  dec_recv->add_callback([&received_count](const dfmessages::TriggerDecision& td) {
    ++received_count;
    // Send a completion token back.
    dfmessages::TriggerDecisionToken token;
    token.run_number = td.run_number;
    token.trigger_number = td.trigger_number;
    token.decision_destination = "trigdec_0";
    get_iom_sender<dfmessages::TriggerDecisionToken>("token")->send(std::move(token), iomanager::Sender::s_block);
  });

  auto inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("triginh");
  inh_recv->add_callback(recv_triginh);

  dfo->execute_command("start", start_data);
  send_init_token(); // Register TRB app with the DFO.

  // Inject a synthetic peer announcement that makes the module believe a
  // second DFO "zzz_peer" is also in the ensemble. "test" < "zzz_peer"
  // alphabetically, so "test" gets index 0 and processes even trigger_numbers.
  {
    dfmessages::DFODecision peer_ann;
    peer_ann.run_number = 0;
    peer_ann.trigger_number = DFOModule::s_peer_announce_magic;
    peer_ann.trb_connection_name = "";
    peer_ann.trb_slot_count = 0;
    peer_ann.source_dfo_name = "zzz_peer";
    peer_ann.is_completion = true;
    get_iom_sender<dfmessages::DFODecision>("dfo_decision_input")
      ->send(std::move(peer_ann), iomanager::Sender::s_block);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Trigger numbers 1–4: only even ones (2, 4) should reach the TRB because
  // this DFO has own_index==0 (trigger_number % 2 == 0).
  for (dfmessages::trigger_number_t tn = 1; tn <= 4; ++tn) {
    send_trigdec(tn);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Exactly 2 decisions (trigger_numbers 2 and 4) should have reached the TRB.
  BOOST_REQUIRE_EQUAL(received_count.load(), 2u);

  dfo->execute_command("drain_dataflow", null_data);
  dfo->execute_command("scrap", null_data);

  dec_recv->remove_callback();
  inh_recv->remove_callback();
}

// ---------------------------------------------------------------------------
// DFODecision broadcast on assignment
//    In standalone mode (no peer DFO connections) no DFODecision output
//    connections are configured, so the broadcast is a no-op.  Verify that
//    the module still correctly processes the TD and the callback path does
//    not throw.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(DFODecisionBroadcastStandaloneNoOp)
{
  auto dfo = appfwk::make_module("DFOModule", "test_consensus");
  opmgr.register_node("dfo", dfo);
  dfo->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  dfo->execute_command("conf", null_data);

  auto iom = iomanager::IOManager::get();
  auto dec_recv = iom->get_receiver<dfmessages::TriggerDecision>("trigdec_0");
  std::atomic<int> received{ 0 };
  dec_recv->add_callback([&](const dfmessages::TriggerDecision& td) {
    ++received;
    dfmessages::TriggerDecisionToken token;
    token.run_number = td.run_number;
    token.trigger_number = td.trigger_number;
    token.decision_destination = "trigdec_0";
    get_iom_sender<dfmessages::TriggerDecisionToken>("token")->send(std::move(token), iomanager::Sender::s_block);
  });

  auto inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("triginh");
  inh_recv->add_callback([](const dfmessages::TriggerInhibit&) {});

  dfo->execute_command("start", start_data);
  send_init_token();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  send_trigdec(1);
  send_trigdec(2);
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Both TDs should have been processed without error.
  BOOST_REQUIRE_GE(received.load(), 1);

  dfo->execute_command("drain_dataflow", null_data);
  dfo->execute_command("scrap", null_data);

  dec_recv->remove_callback();
  inh_recv->remove_callback();
}

// ---------------------------------------------------------------------------
// Watchdog failover
//    Simulate a two-DFO scenario where the responsible peer (zzz_peer) for
//    odd trigger_numbers never sends a DFODecision.  The watchdog should
//    detect the timeout, remove zzz_peer from the ensemble, recompute the
//    partition (this DFO now owns ALL triggers), and reassign the pending TD.
//
//    We use s_dfo_decision_timeout to bound the wait, then verify the module
//    eventually dispatches the previously-orphaned TD.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(WatchdogFailover)
{
  auto dfo = appfwk::make_module("DFOModule", "test_consensus");
  opmgr.register_node("dfo", dfo);
  dfo->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  dfo->execute_command("conf", null_data);

  auto iom = iomanager::IOManager::get();

  std::atomic<uint64_t> received_count{ 0 };
  auto dec_recv = iom->get_receiver<dfmessages::TriggerDecision>("trigdec_0");
  dec_recv->add_callback([&](const dfmessages::TriggerDecision& td) {
    ++received_count;
    dfmessages::TriggerDecisionToken token;
    token.run_number = td.run_number;
    token.trigger_number = td.trigger_number;
    token.decision_destination = "trigdec_0";
    get_iom_sender<dfmessages::TriggerDecisionToken>("token")->send(std::move(token), iomanager::Sender::s_block);
  });

  auto inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("triginh");
  inh_recv->add_callback([](const dfmessages::TriggerInhibit&) {});

  dfo->execute_command("start", start_data);
  send_init_token();

  // Inject a synthetic peer announcement for "zzz_peer" so this DFO gets
  // own_index=0 (even trigger_numbers) and zzz_peer gets index=1 (odd).
  {
    dfmessages::DFODecision peer_ann;
    peer_ann.run_number = 0;
    peer_ann.trigger_number = DFOModule::s_peer_announce_magic;
    peer_ann.trb_connection_name = "";
    peer_ann.trb_slot_count = 0;
    peer_ann.source_dfo_name = "zzz_peer";
    peer_ann.is_completion = true;
    get_iom_sender<dfmessages::DFODecision>("dfo_decision_input")
      ->send(std::move(peer_ann), iomanager::Sender::s_block);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Send trigger_number 1 (odd → would normally go to zzz_peer) and
  // trigger_number 2 (even → processed immediately by this DFO).
  send_trigdec(1); // buffered; waiting for zzz_peer's DFODecision
  send_trigdec(2); // processed immediately

  // Wait for this DFO to handle trigger 2.
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  BOOST_REQUIRE_GE(received_count.load(), 1u); // trigger 2 processed

  // Wait for the watchdog to fire (timeout + extra time for the watchdog
  // interval and processing).
  auto mdal = cfgMgr->get_dal<appmodel::DFOModule>("test_consensus");
  auto dfo_timeout_ms = std::chrono::milliseconds(mdal->get_configuration()->get_dfo_decision_timeout_ms());

  auto watchdog_wait = dfo_timeout_ms + std::chrono::milliseconds(500);
  std::this_thread::sleep_for(watchdog_wait);

  // After failover, trigger 1 should also have been dispatched.
  BOOST_REQUIRE_EQUAL(received_count.load(), 2u);

  dfo->execute_command("drain_dataflow", null_data);
  dfo->execute_command("scrap", null_data);

  dec_recv->remove_callback();
  inh_recv->remove_callback();
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace dunedaq
