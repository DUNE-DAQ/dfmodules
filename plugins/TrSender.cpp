/**
 * @file TrSender.cpp
 *
 * Implementations of TrSender's functions
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "TrSender.hpp"

#include "ers/ers.hpp"
#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/app/Nljs.hpp"
#include "iomanager/IOManager.hpp"
#include "logging/Logging.hpp"
#include "ers/Issue.hpp"
#include "detdataformats/DetID.hpp"
#include "CommonIssues.hpp"

#include "dfmessages/TriggerDecision.hpp"


#include "dfmodules/trsender/Nljs.hpp"
#include "dfmodules/trsenderinfo/InfoNljs.hpp"


#include <fstream>
#include <iostream>
#include <memory>
#include <utility>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <set>
#include <vector>

// for logging
#define TRACE_NAME "TrSender" // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10
#define TVLV_TRIGGER_RECORD 15

namespace dunedaq::dfmodules {

TrSender::TrSender(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&TrSender::do_work, this, std::placeholders::_1))
  , rcthread_(std::bind(&TrSender::do_receive, this, std::placeholders::_1))
  , m_sender()
  , inputQueue_(nullptr)
  , queueTimeout_(100)
{
  register_command("conf", &TrSender::do_conf, std::set<std::string>{ "INITIAL" });
  register_command("start", &TrSender::do_start, std::set<std::string>{ "CONFIGURED" });
  register_command("stop", &TrSender::do_stop, std::set<std::string>{ "TRIGGER_SOURCES_STOPPED"});
  register_command("scrap", &TrSender::do_scrap, std::set<std::string>{ "CONFIGURED" });

}

void
TrSender::init(const data_t& /* structured args */ init_data)
{
TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)  << get_name() << ": Entering init() method";
auto iom = iomanager::IOManager::get();
auto ci = appfwk::connection_index(init_data, { "trigger_record_output", "token_input" });
    m_sender = iom -> get_sender<std::unique_ptr<daqdataformats::TriggerRecord>>(ci["trigger_record_output"]);
    inputQueue_ =  iom -> get_receiver<dfmessages::TriggerDecisionToken>(ci["token_input"]);
TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
TrSender::do_start(const nlohmann::json& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method for TrSender";
  thread_.start_working_thread();
  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method for TrSender";
  }

void
TrSender::do_stop(const nlohmann::json& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  thread_.stop_working_thread();
  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
TrSender::do_scrap(const nlohmann::json& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_unconfigure() method";
  cfg_ = trsender::Conf{}; // reset to defaults
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_unconfigure() method";
}


void
TrSender::do_conf(const nlohmann::json& obj)
{
TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";
auto cfg_ = obj.get<trsender::Conf>();
runNumber = cfg_.runNumber;
triggerCount = cfg_.triggerCount;
dataSize = cfg_.dataSize;
stypeToUse = SourceID::string_to_subsystem(cfg_.stypeToUse);
dtypeToUse = DetID::string_to_subdetector(cfg_.dtypeToUse);
ftypeToUse = string_to_fragment_type(cfg_.ftypeToUse);
elementCount = cfg_.elementCount;
waitBetweenSends = cfg_.waitBetweenSends;

  TLOG() << "\nRun number: " << runNumber << "\nNumber of trigger record: " << triggerCount << "\nNumber of fragments: " << elementCount
         << "\nSubsystem: " << stypeToUse << "\nSubdetector: " << dtypeToUse << "\nFragment type: " << cfg_.ftypeToUse
         << "\nData size: " << dataSize;

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
TrSender::do_work(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  size_t sentCount = 0;
  int receivedConfCount = 0;
  int triggerRecordCount = 0;
  bool successfullyWasSent = false;

    while (!successfullyWasSent && running_flag.load()) {
  
    TLOG_DEBUG(TVLV_TRIGGER_RECORD) << get_name() << ": Receiving configuration ";
    runNumber = cfg_.runNumber;
    triggerCount = cfg_.triggerCount;
    dataSize = cfg_.dataSize;//please note that fragment size is data size + size of header
    stypeToUse = SourceID::string_to_subsystem(cfg_.stypeToUse);
    dtypeToUse = DetID::string_to_subdetector(cfg_.dtypeToUse);
    ftypeToUse = string_to_fragment_type(cfg_.ftypeToUse);
    elementCount = cfg_.elementCount;
    ++receivedConfCount;
    std::ostringstream oss_prog;
    oss_prog << "Received configuration " << receivedConfCount;
    ers::debug(ProgressUpdate(ERS_HERE, "TrSender", oss_prog.str()));

  uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>( // NOLINT(build/unsigned)
                  system_clock::now().time_since_epoch())
                  .count();

  int fragment_size = dataSize + sizeof(FragmentHeader);
  std::vector<char> dummy_data(fragment_size);

    // create TriggerRecordHeader
    TriggerRecordHeaderData trh_data;
    trh_data.trigger_number = triggerCount;
    trh_data.trigger_timestamp = ts;
    trh_data.num_requested_components = elementCount;
    trh_data.run_number = runNumber;
    trh_data.sequence_number = 0;
    trh_data.max_sequence_number = 1;
    trh_data.element_id = SourceID(SourceID::Subsystem::kTRBuilder, 0);//tohle přesně nechápu co je 

    TriggerRecordHeader trh(&trh_data);
    std::unique_ptr<daqdataformats::TriggerRecord> tr = std::make_unique<daqdataformats::TriggerRecord>( trh );
    ++triggerRecordCount;

    // loop over elements=fragments
    for (int ele_num = 0; ele_num < elementCount; ++ele_num) {

      // create our fragment
      FragmentHeader fh;
      fh.trigger_number = triggerCount;
      fh.trigger_timestamp = ts;
      fh.window_begin = ts - 10;
      fh.window_end = ts;
      fh.run_number = runNumber;
      fh.fragment_type = static_cast<fragment_type_t>(ftypeToUse);
      fh.sequence_number = 0;
      fh.detector_id = static_cast<uint16_t>(dtypeToUse);
      fh.element_id = SourceID(stypeToUse, ele_num);

      auto frag_ptr = std::make_unique<Fragment>(dummy_data.data(), dummy_data.size());
      frag_ptr->set_header_fields(fh);

      // add fragment to TriggerRecord
      tr->add_fragment(std::move(frag_ptr));
      } // end loop over elements
  std::ostringstream oss_progr;
  oss_progr << "The trigger record number " << triggerRecordCount << " created.";
  ers::info(ProgressUpdate(ERS_HERE, "TrSender", oss_progr.str()));

        TLOG_DEBUG(TVLV_TRIGGER_RECORD) << get_name() << ": Pushing the trigger record onto queue. ";
            try{
              m_sender->send(std::move(tr), queueTimeout_);
              ++sentCount;
              std::ostringstream oss_progress;
              successfullyWasSent = true;
              oss_progress << "The trigger record number " << triggerRecordCount << " sent.";
              ers::info(ProgressUpdate(ERS_HERE, "TrSender", oss_progress.str()));

          } catch (const dunedaq::iomanager::TimeoutExpired& excpt) {
              std::ostringstream oss_warn;
              oss_warn << "push to output queue \"" ;
              ers::warning(dunedaq::iomanager::TimeoutExpired(
                ERS_HERE,
                "TrSender",
                oss_warn.str(),
                std::chrono::duration_cast<std::chrono::milliseconds>(queueTimeout_).count()));
          }
    TLOG_DEBUG(TVLV_TRIGGER_RECORD) << get_name() << ": Start of sleep between sends";
    std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.waitBetweenSends));
    TLOG_DEBUG(TVLV_TRIGGER_RECORD) << get_name() << ": End of do_work loop";

  
  }
  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, received " << receivedConfCount << " configuration files and successfully sent "
           << sentCount << " trigger records. ";
  ers::info(ProgressUpdate(ERS_HERE, "TrSender", oss_summ.str()));

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

void
TrSender::get_info(opmonlib::InfoCollector& ci, int /* level */)
{
  trsenderinfo::Info info;
  info.configuration_file = receivedConfCount;
  info.trigger_record = sentCount.exchange(0);
  info.tr_created = triggerRecordCount;

  ci.add(info);
}


void
TrSender::do_receive(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
    bool tokenWasSuccessfullyReceived = false;
    while (!tokenWasSuccessfullyReceived && running_flag.load()) {
      try {
        dfmessages::TriggerDecisionToken token;
        token = inputQueue_->receive(queueTimeout_);
        tokenWasSuccessfullyReceived = true;
std::ostringstream oss_progr;
oss_progr << "The token has been received.";
ers::info(ProgressUpdate(ERS_HERE, "TrSender", oss_progr.str()));
      } catch (const dunedaq::iomanager::TimeoutExpired& excpt) {
       continue;
      }}
}
} // namespace dunedaq::dfmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::TrSender)
