/**
 * @file DFOModule_test.cxx Test application that tests and demonstrates
 * the functionality of the DFOModule class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DFOModule.hpp"

#include "dfmessages/TriggerInhibit.hpp"
#include "dfmessages/DataflowStatus.hpp"
#include "dfmessages/DataflowStatusRequest.hpp"
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

std::vector<dfmessages::TriggerDecision> received_decisions;
void
recv_trigdec(const dfmessages::TriggerDecision& decision)
{
  TLOG() << "Received TriggerDecision with trigger number " << decision.trigger_number << " from DFO";
  received_decisions.push_back(decision);
}

std::atomic<bool> busy_signal_recvd = false;
void
recv_triginh(const dfmessages::TriggerInhibit& inhibit)
{
  TLOG() << "Received TriggerInhibit with busy=" << std::boolalpha << inhibit.busy << " from DFO";
  busy_signal_recvd = inhibit.busy;
}

std::unordered_map<dfmessages::trigger_number_t, dfmessages::DataflowStatusRequest> received_status_requests;
void
recv_status_request(const dfmessages::DataflowStatusRequest& request)
{
  TLOG() << "Received DataflowStatusRequest with trigger number " << request.trigger_number << " from DFO";
  received_status_requests[request.trigger_number] = request;
}

void
send_status(dfmessages::DataflowStatus status)
{
  auto iom = iomanager::IOManager::get();
  TLOG() << "Sending DataflowStatus with trigger number " << status.trigger_number;
  iom->get_sender<dfmessages::DataflowStatus>("df_status")->send(std::move(status), iomanager::Sender::s_block);
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
  BOOST_REQUIRE_EQUAL(metric.statuses_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.forwarding_decision(), 0);
  BOOST_REQUIRE_EQUAL(metric.waiting_for_decision(), 0);
  BOOST_REQUIRE_EQUAL(metric.deciding_destination(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_completed(), 0);
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 0);
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
  auto req_recv = iom->get_receiver<dfmessages::DataflowStatusRequest>("df_status_request");
  req_recv->add_callback(recv_status_request);

  send_trigdec(1, true);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Note: Counters are reset by calling get_dfo_info!
  auto metric = get_dfo_info();

  BOOST_REQUIRE_EQUAL(metric.statuses_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_completed(), 0);
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 0);

  dfo->execute_command("start", start_data);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  metric = get_dfo_info();
  BOOST_REQUIRE_EQUAL(metric.statuses_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_completed(), 0);
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 0);

  send_trigdec(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  BOOST_REQUIRE_EQUAL(busy_signal_recvd.load(), true);

  metric = get_dfo_info();
  BOOST_REQUIRE_EQUAL(metric.statuses_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_completed(), 0);
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 0);

  dfmessages::DataflowStatus status;
  status.decision_destination = "trigdec_0";
  status.request_destination = "df_status_request";
  status.trigger_number = 0;
  status.run_number = 1;

  status.trigger_type_mask = 0xFFFFFFFF;
  status.is_busy = false;
  status.busy_threshold = 1;
  status.free_threshold = 0;

  send_status(status);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  BOOST_REQUIRE_EQUAL(busy_signal_recvd.load(), true);

  metric = get_dfo_info();
  BOOST_REQUIRE_EQUAL(metric.statuses_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_completed(), 0);
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 0);

  BOOST_REQUIRE_EQUAL(received_status_requests.size(), 1);
  BOOST_REQUIRE(received_status_requests.find(1) != received_status_requests.end());
  BOOST_REQUIRE_EQUAL(received_status_requests[1].reply_destination, "df_status");

  status.trigger_number = 1;
  send_status(status);
  BOOST_REQUIRE_EQUAL(busy_signal_recvd.load(), true);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  BOOST_REQUIRE_EQUAL(busy_signal_recvd.load(), false);

  metric = get_dfo_info();
  BOOST_REQUIRE_EQUAL(metric.statuses_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 1);
  BOOST_REQUIRE_EQUAL(metric.decisions_completed(), 0);
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 1);

  BOOST_REQUIRE_EQUAL(received_decisions.size(), 1);
  BOOST_REQUIRE_EQUAL(received_decisions[0].trigger_number, 1);
  status.trigger_number = 0;
  status.triggers_building.insert(1);
  status.is_busy = true;
  received_decisions.clear();

  send_status(status);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  BOOST_REQUIRE_EQUAL(busy_signal_recvd.load(), true);

  metric = get_dfo_info();
  BOOST_REQUIRE_EQUAL(metric.statuses_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_completed(), 0);
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 1);

  status.triggers_building.clear();
  status.recently_completed_triggers.insert(1);
  status.is_busy = false;
  send_status(status);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  BOOST_REQUIRE_EQUAL(busy_signal_recvd.load(), false);

  metric = get_dfo_info();
  BOOST_REQUIRE_EQUAL(metric.statuses_received(), 1);
  BOOST_REQUIRE_EQUAL(metric.decisions_received(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_sent(), 0);
  BOOST_REQUIRE_EQUAL(metric.decisions_completed(), 1);
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 0);

  auto start_time = std::chrono::steady_clock::now();
  dfo->execute_command("drain_dataflow", null_data);
  dfo->execute_command("scrap", null_data);
  BOOST_REQUIRE(std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(100));

  dec_recv->remove_callback();
  inh_recv->remove_callback();
  req_recv->remove_callback();
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace dunedaq
