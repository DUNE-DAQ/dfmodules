/**
 * @file TRBModule_test.cxx Test application that tests and demonstrates
 * the functionality of the TRBModule class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "TRBModule.hpp"

#include "daqdataformats/SourceID.hpp"
#include "dfmessages/DataRequest.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/TRBCompletion.hpp"
#include "dfmessages/TriggerRecord_serialization.hpp"
#include "dfmessages/Fragment_serialization.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/opmon/TRBModule.pb.h"
#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"
#include "opmonlib/TestOpMonManager.hpp"

#define BOOST_TEST_MODULE TRBModule_test // NOLINT

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
	std::string oksConfig = "oksconflibs:test/config/triggerrecordbuilder_test.data.xml";
	std::string appName = "TestApp";
	std::string sessionName = "partition_name";
	cfgMgr = std::make_shared<dunedaq::appfwk::ConfigurationManager>(oksConfig, appName, sessionName);
	get_iomanager()->configure(sessionName, cfgMgr->get_queues(), cfgMgr->get_networkconnections(), nullptr, opmgr);
  }
  ~CfgFixture() { get_iomanager()->reset(); }

  auto get_trb_info()
  {
	opmgr.collect();
	auto opmon_facility = opmgr.get_backend_facility();
	auto list = opmon_facility->get_entries(std::regex(".*TRBInfo"));
	BOOST_REQUIRE_EQUAL(list.size(), 1);
	const auto& entry = list.front();
	return opmonlib::from_entry<dfmodules::opmon::TRBInfo>(entry);
  }
  auto get_trb_errors()
  {
    opmgr.collect();
    auto opmon_facility = opmgr.get_backend_facility();
    auto list = opmon_facility->get_entries(std::regex(".*TRBErrors"));
    BOOST_REQUIRE_EQUAL(list.size(), 1);
    const auto& entry = list.front();
    return opmonlib::from_entry<dfmodules::opmon::TRBErrors>(entry);
  }

  dunedaq::opmonlib::TestOpMonManager opmgr;
  std::shared_ptr<dunedaq::appfwk::ConfigurationManager> cfgMgr;
};

BOOST_FIXTURE_TEST_SUITE(TRBModule_test, CfgFixture)

// Storage for received messages
std::vector<std::unique_ptr<daqdataformats::TriggerRecord>> received_trigger_records;
std::vector<dfmessages::DataRequest> received_data_requests;
std::vector<dfmessages::TRBCompletion> received_trb_completes;

void
recv_trigrecord(std::unique_ptr<daqdataformats::TriggerRecord>& tr)
{
  TLOG() << "Received TriggerRecord with trigger number " << tr->get_header_ref().get_trigger_number();
  received_trigger_records.push_back(std::move(tr));
}

void
recv_datareq(const dfmessages::DataRequest& req)
{
  TLOG() << "Received DataRequest for trigger number " << req.trigger_number;
  received_data_requests.push_back(req);
}

void
recv_trbcomplete(const dfmessages::TRBCompletion& complete)
{
  TLOG() << "Received TRBCompletion for trigger number " << complete.trigger_id.trigger_number;
  received_trb_completes.push_back(complete);
}

void
send_trigdec(dfmessages::trigger_number_t trigger_number, dfmessages::run_number_t run_number = 1, int window_size = 2000)
{
  dunedaq::dfmessages::TriggerDecision td;
  td.trigger_number = trigger_number;
  td.run_number = run_number;
  td.trigger_timestamp = 50000 + trigger_number * 1000;
  td.trigger_type = 1;
  td.readout_type = dunedaq::dfmessages::ReadoutType::kLocalized;

  // Add component request (simplified)
  dfmessages::ComponentRequest comp_req;
  comp_req.component.subsystem = daqdataformats::SourceID::Subsystem::kDetectorReadout;
  comp_req.component.id = 0;
  comp_req.window_begin = td.trigger_timestamp;
  comp_req.window_end = td.trigger_timestamp + window_size;
  td.components.push_back(comp_req);

  auto iom = iomanager::IOManager::get();
  TLOG() << "Sending TriggerDecision with trigger number " << trigger_number << " to TRB";
  iom->get_sender<dfmessages::TriggerDecision>("trigdec_trb")->send(std::move(td), iomanager::Sender::s_block);
}

void
send_fragment(dfmessages::trigger_number_t trigger_number,
			  dfmessages::run_number_t run_number = 1,
			  daqdataformats::sequence_number_t sequence_number = 0)
{
  daqdataformats::FragmentHeader hdr;
  hdr.trigger_number = trigger_number;
  hdr.run_number = run_number;
  hdr.sequence_number = sequence_number;
  hdr.trigger_timestamp = 50000 + trigger_number * 1000;
  hdr.window_begin = hdr.trigger_timestamp + (sequence_number * 5000);
  hdr.window_end = hdr.trigger_timestamp + ((sequence_number + 1) * 5000);
  hdr.element_id = daqdataformats::SourceID(daqdataformats::SourceID::Subsystem::kDetectorReadout, 0);
  hdr.fragment_type = static_cast<daqdataformats::fragment_type_t>(daqdataformats::FragmentType::kWIBEth);
  hdr.size = sizeof(daqdataformats::FragmentHeader);

  auto frag = std::make_unique<daqdataformats::Fragment>(&hdr, dunedaq::daqdataformats::Fragment::BufferAdoptionMode::kCopyFromBuffer);

  auto iom = iomanager::IOManager::get();
  TLOG() << "Sending Fragment for trigger number " << trigger_number << " to TRB";
  iom->get_sender<std::unique_ptr<daqdataformats::Fragment>>("fragment")->send(std::move(frag), iomanager::Sender::s_block);
}

BOOST_AUTO_TEST_CASE(CopyAndMoveSemantics)
{
  BOOST_REQUIRE(!std::is_copy_constructible_v<TRBModule>);
  BOOST_REQUIRE(!std::is_copy_assignable_v<TRBModule>);
  BOOST_REQUIRE(!std::is_move_constructible_v<TRBModule>);
  BOOST_REQUIRE(!std::is_move_assignable_v<TRBModule>);
}

BOOST_AUTO_TEST_CASE(Constructor)
{
  TLOG() << "Test case Constructor BEGIN";
  auto trb = appfwk::make_module("TRBModule", "test");
  TLOG() << "Test case Constructor END";
}

BOOST_AUTO_TEST_CASE(Init)
{
  TLOG() << "Test case Init BEGIN";
  auto trb = appfwk::make_module("TRBModule", "test");
  trb->init(cfgMgr);
  TLOG() << "Test case Init END";
}

BOOST_AUTO_TEST_CASE(Commands)
{
  TLOG() << "Test case Commands BEGIN";
  auto trb = appfwk::make_module("TRBModule", "test");
  opmgr.register_node("trb", trb);
  trb->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  trb->execute_command("conf", null_data);
  trb->execute_command("start", start_data);
  trb->execute_command("stop", null_data);
  trb->execute_command("scrap", null_data);

  auto metric = get_trb_info();
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 0);
  BOOST_REQUIRE_EQUAL(metric.pending_fragments(), 0);
  TLOG() << "Test case Commands END";
}

BOOST_AUTO_TEST_CASE(DataFlow)
{
  TLOG() << "Test case DataFlow BEGIN";
  auto trb = appfwk::make_module("TRBModule", "test");
  opmgr.register_node("trb", trb);
  trb->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  trb->execute_command("conf", null_data);

  auto iom = iomanager::IOManager::get();
  auto tr_recv = iom->get_receiver<std::unique_ptr<daqdataformats::TriggerRecord>>("trigrecord");
  tr_recv->add_callback(recv_trigrecord);
  auto dr_recv = iom->get_receiver<dfmessages::DataRequest>("datareq_ru0");
  dr_recv->add_callback(recv_datareq);
  auto trbc_recv = iom->get_receiver<dfmessages::TRBCompletion>("trb_complete");
  trbc_recv->add_callback(recv_trbcomplete);

  received_trb_completes.clear();
  received_trigger_records.clear();
  received_data_requests.clear();

  trb->execute_command("start", start_data);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  auto metric = get_trb_info();
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 0);
  BOOST_REQUIRE_EQUAL(metric.pending_fragments(), 0);

  // Send a trigger decision
  send_trigdec(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  metric = get_trb_info();
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 1);
  BOOST_REQUIRE_EQUAL(metric.pending_fragments(), 1);

  // Should have received a data request
  BOOST_REQUIRE_EQUAL(received_data_requests.size(), 1);
  BOOST_REQUIRE_EQUAL(received_data_requests[0].trigger_number, 1);

  // Send corresponding fragment
  send_fragment(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Should have received a trigger record
  BOOST_REQUIRE_EQUAL(received_trigger_records.size(), 1);
  BOOST_REQUIRE_EQUAL(received_trigger_records[0]->get_header_ref().get_trigger_number(), 1);
  BOOST_REQUIRE_EQUAL(received_trigger_records[0]->get_header_ref().get_run_number(), 1);

  // Should have recieved a TRBCompletion message
  BOOST_REQUIRE_EQUAL(received_trb_completes.size(), 1);
  BOOST_REQUIRE_EQUAL(received_trb_completes[0].trigger_id.trigger_number, 1);

  metric = get_trb_info();
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 0);
  BOOST_REQUIRE_EQUAL(metric.pending_fragments(), 0);

  trb->execute_command("stop", null_data);
  trb->execute_command("scrap", null_data);

  trbc_recv->remove_callback();
  tr_recv->remove_callback();
  dr_recv->remove_callback();
  TLOG() << "Test case DataFlow END";
}

BOOST_AUTO_TEST_CASE(WrongRunNumber)
{
  TLOG() << "Test case WrongRunNumber BEGIN";
  auto trb = appfwk::make_module("TRBModule", "test");
  opmgr.register_node("trb", trb);
  trb->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  trb->execute_command("conf", null_data);

  auto iom = iomanager::IOManager::get();
  auto tr_recv = iom->get_receiver<std::unique_ptr<daqdataformats::TriggerRecord>>("trigrecord");
  tr_recv->add_callback(recv_trigrecord);
  auto dr_recv = iom->get_receiver<dfmessages::DataRequest>("datareq_ru0");
  dr_recv->add_callback(recv_datareq);
  auto trbc_recv = iom->get_receiver<dfmessages::TRBCompletion>("trb_complete");
  trbc_recv->add_callback(recv_trbcomplete);

  received_trb_completes.clear();
  received_trigger_records.clear();
  received_data_requests.clear();

  trb->execute_command("start", start_data);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Send trigger decision with wrong run number
  send_trigdec(1, 999);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Should not create any pending trigger decisions (rejected)
  auto metric = get_trb_info();
  auto err_metric = get_trb_errors();
  BOOST_REQUIRE_EQUAL(metric.received_trigger_decisions(), 0);
  BOOST_REQUIRE_EQUAL(err_metric.unexpected_trigger_decisions(), 1);

  trb->execute_command("stop", null_data);
  trb->execute_command("scrap", null_data);

  trbc_recv->remove_callback();
  tr_recv->remove_callback();
  dr_recv->remove_callback();
  TLOG() << "Test case WrongRunNumber END";
}

BOOST_AUTO_TEST_CASE(FragmentTimeout)
{
  TLOG() << "Test case FragmentTimeout BEGIN";
  auto trb = appfwk::make_module("TRBModule", "test");
  opmgr.register_node("trb", trb);
  trb->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  trb->execute_command("conf", null_data);

  auto iom = iomanager::IOManager::get();
  auto tr_recv = iom->get_receiver<std::unique_ptr<daqdataformats::TriggerRecord>>("trigrecord");
  tr_recv->add_callback(recv_trigrecord);
  auto dr_recv = iom->get_receiver<dfmessages::DataRequest>("datareq_ru0");
  dr_recv->add_callback(recv_datareq);
  auto trbc_recv = iom->get_receiver<dfmessages::TRBCompletion>("trb_complete");
  trbc_recv->add_callback(recv_trbcomplete);

  received_trb_completes.clear();
  received_trigger_records.clear();
  received_data_requests.clear();

  trb->execute_command("start", start_data);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Send a trigger decision
  send_trigdec(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  auto metric = get_trb_info();
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 1);
  BOOST_REQUIRE_EQUAL(metric.pending_fragments(), 1);

  // Should have received a data request
  BOOST_REQUIRE_EQUAL(received_data_requests.size(), 1);
  BOOST_REQUIRE_EQUAL(received_data_requests[0].trigger_number, 1);

  // Send corresponding fragment
  send_fragment(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Should have received a trigger record
  BOOST_REQUIRE_EQUAL(received_trigger_records.size(), 1);
  BOOST_REQUIRE_EQUAL(received_trigger_records[0]->get_header_ref().get_trigger_number(), 1);
  BOOST_REQUIRE_EQUAL(received_trigger_records[0]->get_header_ref().get_run_number(), 1);

  // Should have recieved a TRBCompletion message
  BOOST_REQUIRE_EQUAL(received_trb_completes.size(), 1);
  BOOST_REQUIRE_EQUAL(received_trb_completes[0].trigger_id.trigger_number, 1);

  metric = get_trb_info();
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 0);
  BOOST_REQUIRE_EQUAL(metric.pending_fragments(), 0);

  // Send trigger decision
  send_trigdec(2);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  metric = get_trb_info();
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 1);

  // Wait for timeout (configured to 1000ms)
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));
  send_fragment(999); // Send fragment for a different trigger number to avoid completing the trigger record, but process for stale
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Should have timed out and sent incomplete trigger record
  metric = get_trb_info();
  auto err_metric = get_trb_errors();
  BOOST_REQUIRE_EQUAL(err_metric.timed_out_trigger_records(), 1);
  BOOST_REQUIRE_EQUAL(received_trigger_records.size(), 2);

  // Should have recieved a TRBCompletion message
  BOOST_REQUIRE_EQUAL(received_trb_completes.size(), 2);
  BOOST_REQUIRE_EQUAL(received_trb_completes[1].trigger_id.trigger_number, 2);

  trb->execute_command("stop", null_data);
  trb->execute_command("scrap", null_data);

  trbc_recv->remove_callback();
  tr_recv->remove_callback();
  dr_recv->remove_callback();
  TLOG() << "Test case FragmentTimeout END";
}

BOOST_AUTO_TEST_CASE(UnexpectedFragment)
{
  TLOG() << "Test case UnexpectedFragment BEGIN";
  auto trb = appfwk::make_module("TRBModule", "test");
  opmgr.register_node("trb", trb);
  trb->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  trb->execute_command("conf", null_data);

  auto iom = iomanager::IOManager::get();
  auto tr_recv = iom->get_receiver<std::unique_ptr<daqdataformats::TriggerRecord>>("trigrecord");
  tr_recv->add_callback(recv_trigrecord);
  auto dr_recv = iom->get_receiver<dfmessages::DataRequest>("datareq_ru0");
  dr_recv->add_callback(recv_datareq);
  auto trbc_recv = iom->get_receiver<dfmessages::TRBCompletion>("trb_complete");
  trbc_recv->add_callback(recv_trbcomplete);

  received_trb_completes.clear();
  received_trigger_records.clear();
  received_data_requests.clear();

  trb->execute_command("start", start_data);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Send fragment without trigger decision
  send_fragment(999);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Should be counted as unexpected
  auto metric = get_trb_info();
  auto err_metric = get_trb_errors();
  BOOST_REQUIRE_EQUAL(err_metric.unexpected_fragments(), 1);
  BOOST_REQUIRE_EQUAL(received_trigger_records.size(), 0);

  trb->execute_command("stop", null_data);
  trb->execute_command("scrap", null_data);

  trbc_recv->remove_callback();
  tr_recv->remove_callback();
  dr_recv->remove_callback();
  TLOG() << "Test case UnexpectedFragment END";
}

BOOST_AUTO_TEST_CASE(MultipleTriggers)
{
  TLOG() << "Test case MultipleTriggers BEGIN";
  auto iom = iomanager::IOManager::get();
  auto tr_recv = iom->get_receiver<std::unique_ptr<daqdataformats::TriggerRecord>>("trigrecord");
  tr_recv->add_callback(recv_trigrecord);
  auto dr_recv = iom->get_receiver<dfmessages::DataRequest>("datareq_ru0");
  dr_recv->add_callback(recv_datareq);
  auto trbc_recv = iom->get_receiver<dfmessages::TRBCompletion>("trb_complete");
  trbc_recv->add_callback(recv_trbcomplete);
  auto trb = appfwk::make_module("TRBModule", "test");
  opmgr.register_node("trb", trb);
  trb->init(cfgMgr);


  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  trb->execute_command("conf", null_data);


  received_trb_completes.clear();
  received_trigger_records.clear();
  received_data_requests.clear();

  trb->execute_command("start", start_data);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Send multiple trigger decisions
  for (int i = 1; i <= 5; ++i) {
	send_trigdec(i);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  auto metric = get_trb_info();
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 5);
  BOOST_REQUIRE_EQUAL(received_data_requests.size(), 5);

  // Send all fragments
  for (int i = 1; i <= 5; ++i) {
	send_fragment(i);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // All should complete
  BOOST_REQUIRE_EQUAL(received_trigger_records.size(), 5);
  BOOST_REQUIRE_EQUAL(received_trb_completes.size(), 5);
  metric = get_trb_info();
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 0);

  trb->execute_command("stop", null_data);
  trb->execute_command("scrap", null_data);

  trbc_recv->remove_callback();
  tr_recv->remove_callback();
  dr_recv->remove_callback();
  TLOG() << "Test case MultipleTriggers END";
}

BOOST_AUTO_TEST_CASE(MultipleSequences)
{
  TLOG() << "Test case MultipleSequences START";
  auto trb = appfwk::make_module("TRBModule", "test");
  opmgr.register_node("trb", trb);
  trb->init(cfgMgr);

  appfwk::DAQModule::CommandData_t null_data;
  appfwk::DAQModule::CommandData_t start_data;
  start_data.emplace("run", 1);

  trb->execute_command("conf", null_data);

  auto iom = iomanager::IOManager::get();
  auto tr_recv = iom->get_receiver<std::unique_ptr<daqdataformats::TriggerRecord>>("trigrecord");
  tr_recv->add_callback(recv_trigrecord);
  auto dr_recv = iom->get_receiver<dfmessages::DataRequest>("datareq_ru0");
  dr_recv->add_callback(recv_datareq);
  auto trbc_recv = iom->get_receiver<dfmessages::TRBCompletion>("trb_complete");
  trbc_recv->add_callback(recv_trbcomplete);

  received_trb_completes.clear();
  received_trigger_records.clear();
  received_data_requests.clear();

  trb->execute_command("start", start_data);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Create a trigger decision with a large time window that will be split into multiple sequences
  // max_sequence_length_ticks is set to 5000 in config
  // We'll create a window of 12000 ticks, which should result in 3 sequences:
  // sequence 0: 0-5000, sequence 1: 5000-10000, sequence 2: 10000-12000
  send_trigdec(100, 1, 12000);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Should have received 3 data requests (one per sequence)
  BOOST_REQUIRE_EQUAL(received_data_requests.size(), 3);
  TLOG() << "Received " << received_data_requests.size() << " data requests (one per sequence)";

  // Verify sequence numbers in data requests
  std::set<daqdataformats::sequence_number_t> request_sequences;
  for (const auto& req : received_data_requests) {
    BOOST_REQUIRE_EQUAL(req.trigger_number, 100);
    request_sequences.insert(req.sequence_number);
    TLOG() << "DataRequest: trigger=" << req.trigger_number
           << ", sequence=" << req.sequence_number
           << ", window=" << req.request_information.window_begin
           << "-" << req.request_information.window_end;
  }
  BOOST_REQUIRE_EQUAL(request_sequences.size(), 3);
  BOOST_REQUIRE(request_sequences.count(0) > 0);
  BOOST_REQUIRE(request_sequences.count(1) > 0);
  BOOST_REQUIRE(request_sequences.count(2) > 0);

  auto metric = get_trb_info();
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 3);  // 3 pending sequences
  BOOST_REQUIRE_EQUAL(metric.pending_fragments(), 3);

  // Send fragments for all three sequences
  TLOG() << "Sending fragments for all 3 sequences";
  for (daqdataformats::sequence_number_t seq = 0; seq < 3; ++seq) {
    send_fragment(100, 1, seq);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Should have received 3 trigger records (one per sequence)
  BOOST_REQUIRE_EQUAL(received_trigger_records.size(), 3);
  TLOG() << "Received " << received_trigger_records.size() << " TriggerRecords (one per sequence)";

  // Verify the trigger records
  std::set<daqdataformats::sequence_number_t> tr_sequences;
  for (const auto& tr : received_trigger_records) {
    auto& hdr = tr->get_header_ref();
    BOOST_REQUIRE_EQUAL(hdr.get_trigger_number(), 100);
    BOOST_REQUIRE_EQUAL(hdr.get_run_number(), 1);
    BOOST_REQUIRE_EQUAL(hdr.get_max_sequence_number(), 2);  // max sequence is 2 (0, 1, 2)

    tr_sequences.insert(hdr.get_sequence_number());

    TLOG() << "TriggerRecord: trigger=" << hdr.get_trigger_number()
           << ", sequence=" << hdr.get_sequence_number()
           << ", max_sequence=" << hdr.get_max_sequence_number()
           << ", fragments=" << tr->get_fragments_ref().size();

    // Each sequence should have one fragment
    BOOST_REQUIRE_EQUAL(tr->get_fragments_ref().size(), 1);
  }

  // Verify we got all three sequences
  BOOST_REQUIRE_EQUAL(tr_sequences.size(), 3);
  BOOST_REQUIRE(tr_sequences.count(0) > 0);
  BOOST_REQUIRE(tr_sequences.count(1) > 0);
  BOOST_REQUIRE(tr_sequences.count(2) > 0);

  // Should have received 1 TRBCompletion (after all sequences complete)
  BOOST_REQUIRE_EQUAL(received_trb_completes.size(), 3);
  BOOST_REQUIRE_EQUAL(received_trb_completes[0].trigger_id.trigger_number, 100);
  BOOST_REQUIRE_EQUAL(received_trb_completes[0].trigger_record_max_sequence_number, 2);
  TLOG() << "Received TRBCompletion with max_sequence_number="
         << received_trb_completes[0].trigger_record_max_sequence_number;

  metric = get_trb_info();
  BOOST_REQUIRE_EQUAL(metric.pending_trigger_decisions(), 0);
  BOOST_REQUIRE_EQUAL(metric.pending_fragments(), 0);

  trb->execute_command("stop", null_data);
  trb->execute_command("scrap", null_data);

  trbc_recv->remove_callback();
  tr_recv->remove_callback();
  dr_recv->remove_callback();
  TLOG() << "Test case MultipleSequences END";
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace dunedaq
