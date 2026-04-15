/**
 * @file DFOConsensusModule_test.cxx Unit tests for DFOConsensusModule.
 *
 * Tests cover:
 *  1. Copy/move semantics
 *  2. Constructor and plugin creation
 *  3. Standalone mode: zero peer connections → behaves identically to DFOModule
 *  4. Partition-filter logic: trigger decisions belonging to a different
 *     partition are silently dropped, while decisions for the own partition
 *     are processed normally.
 *  5. Peer-announcement magic value does not collide with normal TRB tokens.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DFOConsensusModule.hpp"

#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TriggerInhibit.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/opmon/DFOModule.pb.h"
#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"
#include "opmonlib/TestOpMonManager.hpp"

#define BOOST_TEST_MODULE DFOConsensusModule_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace dunedaq::dfmodules;

namespace dunedaq {

struct EnvFixture
{
  EnvFixture() { setenv("DUNEDAQ_PARTITION", "partition_name", 0); }
};
BOOST_TEST_GLOBAL_FIXTURE(EnvFixture);

// ---------------------------------------------------------------------------
// Fixture – reuses the same test config as DFOModule_test (single-DFO, no
// peer connections).  DFOConsensusModule falls back to standalone mode and
// should be indistinguishable from DFOModule.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Helpers (shared with DFOModule_test style)
// ---------------------------------------------------------------------------
void
send_init_token(std::string connection_name = "trigdec_0")
{
  dfmessages::TriggerDecisionToken token;
  token.run_number = 0;
  token.trigger_number = 0;
  token.decision_destination = connection_name;
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
  get_iom_sender<dfmessages::TriggerDecisionToken>("token")->send(std::move(token), iomanager::Sender::s_block);
}

void
recv_trigdec(const dfmessages::TriggerDecision& decision)
{
  TLOG() << "Received TriggerDecision with trigger number " << decision.trigger_number;
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  send_token(decision.trigger_number);
}

std::atomic<bool> busy_signal_recvd = false;
void
recv_triginh(const dfmessages::TriggerInhibit& inhibit)
{
  TLOG() << "Received TriggerInhibit with busy=" << std::boolalpha << inhibit.busy;
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
  iomanager::IOManager::get()->get_sender<dfmessages::TriggerDecision>("trigdec")->send(
    std::move(td), iomanager::Sender::s_block);
}

// ===========================================================================
BOOST_FIXTURE_TEST_SUITE(DFOConsensusModule_test, CfgFixture)
// ===========================================================================

// ---------------------------------------------------------------------------
// 1. Copy/move semantics
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(CopyAndMoveSemantics)
{
  BOOST_REQUIRE(!std::is_copy_constructible_v<DFOConsensusModule>);
  BOOST_REQUIRE(!std::is_copy_assignable_v<DFOConsensusModule>);
  BOOST_REQUIRE(!std::is_move_constructible_v<DFOConsensusModule>);
  BOOST_REQUIRE(!std::is_move_assignable_v<DFOConsensusModule>);
}

// ---------------------------------------------------------------------------
// 2. Constructor / plugin creation
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(Constructor)
{
  auto dfo = appfwk::make_module("DFOConsensusModule", "test");
  BOOST_REQUIRE(dfo != nullptr);
}

// ---------------------------------------------------------------------------
// 3. Init with the existing test config (no peer connections)
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(Init)
{
  auto dfo = appfwk::make_module("DFOConsensusModule", "test");
  dfo->init(cfgMgr);
}

// ---------------------------------------------------------------------------
// 4. Full command lifecycle in standalone mode (no peers)
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(Commands)
{
  auto dfo = appfwk::make_module("DFOConsensusModule", "test");
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
}

// ---------------------------------------------------------------------------
// 5. Standalone data-flow (identical to DFOModule_test::DataFlow)
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(DataFlowStandaloneMode)
{
  auto dfo = appfwk::make_module("DFOConsensusModule", "test");
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

  dfo->execute_command("start", start_data);
  send_init_token();

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  // In standalone mode (num_dfos==1) every trigger decision is processed.
  send_trigdec(2);
  send_trigdec(3);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  send_trigdec(4);

  auto metric = get_dfo_info();
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

// ---------------------------------------------------------------------------
// 6. Peer-announcement magic value
//    Verify that s_peer_announce_magic does not equal 0 (which is the
//    trigger_number used in TRB registration tokens) and is max for the type.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(PeerAnnounceMagicValue)
{
  BOOST_REQUIRE_NE(DFOConsensusModule::s_peer_announce_magic,
                   static_cast<daqdataformats::trigger_number_t>(0));
  BOOST_REQUIRE_EQUAL(DFOConsensusModule::s_peer_announce_magic,
                      std::numeric_limits<daqdataformats::trigger_number_t>::max());
}

// ---------------------------------------------------------------------------
// 7. Partition-filter logic (unit test without networking)
//    Instantiate the DFOConsensusModule and manually exercise the filter by
//    sending trigger decisions at various trigger_numbers. With two DFOs
//    (indices 0 and 1), decisions with even trigger_numbers go to the DFO
//    at index 0 and odd ones to index 1.
//    Here we simulate the DFO that has own_index=0 and num_dfos=2 by
//    starting it and injecting a synthetic peer announcement so the partition
//    settles before the first trigger decision arrives.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(PartitionFilter)
{
  auto dfo = appfwk::make_module("DFOConsensusModule", "test");
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
    get_iom_sender<dfmessages::TriggerDecisionToken>("token")->send(
      std::move(token), iomanager::Sender::s_block);
  });

  auto inh_recv = iom->get_receiver<dfmessages::TriggerInhibit>("triginh");
  inh_recv->add_callback(recv_triginh);

  dfo->execute_command("start", start_data);
  send_init_token(); // Register TRB app with the DFO.

  // Inject a synthetic peer announcement that makes the module believe a
  // second DFO "zzz_peer" is also in the ensemble. "test" < "zzz_peer"
  // alphabetically, so "test" gets index 0 and processes even trigger_numbers.
  {
    dfmessages::TriggerDecisionToken peer_ann;
    peer_ann.run_number = 0;
    peer_ann.trigger_number = DFOConsensusModule::s_peer_announce_magic;
    peer_ann.decision_destination = "zzz_peer";
    get_iom_sender<dfmessages::TriggerDecisionToken>("token")->send(
      std::move(peer_ann), iomanager::Sender::s_block);
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

BOOST_AUTO_TEST_SUITE_END()

} // namespace dunedaq
