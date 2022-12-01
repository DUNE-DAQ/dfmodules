/**
 * @file Receiver.cpp
 *
 * Implementations of Receiver's functions
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "Receiver.hpp"
#include "CommonIssues.hpp"

#include "dfmodules/receiverinfo/InfoNljs.hpp"
#include "appfwk/DAQModuleHelper.hpp"
#include "iomanager/IOManager.hpp"
#include "logging/Logging.hpp"
#include "ers/Issue.hpp"
#include "daqdataformats/TriggerRecord.hpp"

#include <chrono>
#include <functional>
#include <thread>
#include <string>

//for logging
#define TRACE_NAME "Receiver" //NOLINT
#define TLVL_ENTER_EXIT_METHODS 10
#define TLVL_QUEUE 15


namespace dunedaq::dfmodules {

Receiver::Receiver(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
, thread_(std::bind(&Receiver::do_work, this, std::placeholders::_1))
//, m_receiver(nullptr)
//, queueTimeout_(100)
{
  register_command("start", &Receiver::do_start);
  register_command("stop", &Receiver::do_stop);
}

void
Receiver::init(const data_t& /* structured args */ iniobj)
{
TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
auto ci = appfwk::connection_index(iniobj, { "trigger_record_input" });
/* try {
    m_receiver = get_iom_receiver<std::unique_ptr<daqdataformats::TriggerRecord>>(ci["trigger_record_input"]);
  } catch (const ers::Issue& excpt) {
    throw dunedaq::dfmodule::InvalidQueueFatalError(ERS_HERE, get_name(), "trigger_record_input", excpt);
  }
*/TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
Receiver::do_start(const nlohmann::json& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  thread_.start_working_thread();
  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
Receiver::do_stop(const nlohmann::json& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  thread_.stop_working_thread();
  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
Receiver::do_work(std::atomic<bool>& running_flag)
{
/*  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int receivedCount = 0;
  std::unique_ptr<daqdataformats::TriggerRecord> element{ nullptr };
  while (running_flag.load()) {
    bool trWasSuccessfullyReceived = false;
   while (!trWasSuccessfullyReceived && running_flag.load()) {
      TLOG_DEBUG(TLVL_QUEUE) << get_name() << ": Going to receive data from the input queue";
    try{
    element = m_receiver->receive(queueTimeout_);
    ++receivedCount;
    trWasSuccessfullyReceived = true;
    std::ostringstream oss_progr;
    oss_progr << "The trigger record number " << receivedCount << " received.";
    ers::info(ProgressUpdate(ERS_HERE, "TrSender", oss_progr.str()));

    } catch (const dunedaq::iomanager::TimeoutExpired& excpt) {
      continue;        }
    }   
TLOG_DEBUG(TLVL_QUEUE) << get_name() << ": Received trigger record pointer" << element;

}

  std::ostringstream oss_summ;
  oss_summ << ": Exiting do_work() method, received " << receivedCount << " trigger records";
  ers::info(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";

*/}

void
Receiver::get_info(opmonlib::InfoCollector& ci, int /* level */)
{
 receiverinfo::Info info;
  info.trigger_record = receivedCount;
    ci.add(info);
}

} // namespace dunedaq::dfmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::Receiver)
