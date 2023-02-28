/**
 * @file TrSender.hpp
 *
 * Developer(s) of this DAQModule have yet to replace this line with a brief description of the DAQModule.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_TRSENDER_HPP_
#define DFMODULES_PLUGINS_TRSENDER_HPP_

#include "appfwk/DAQModule.hpp"
#include "iomanager/Sender.hpp"
#include "iomanager/Receiver.hpp" 
#include "utilities/WorkerThread.hpp"
#include "dfmodules/trsender/Structs.hpp"
#include "daqdataformats/TriggerRecord.hpp"
#include "daqdataformats/SourceID.hpp"
#include "detdataformats/DetID.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace dunedaq::daqdataformats;
using namespace dunedaq::detdataformats;

namespace dunedaq {
namespace dfmodules {

class TrSender : public dunedaq::appfwk::DAQModule
{
public:
  explicit TrSender(const std::string& name);

  void init(const data_t&) override;
  void get_info(opmonlib::InfoCollector&, int /*level*/) override;

  TrSender(const TrSender&) = delete;
  TrSender& operator=(const TrSender&) = delete;
  TrSender(TrSender&&) = delete;
  TrSender& operator=(TrSender&&) = delete;

  ~TrSender() = default;

private:
  //Commands
  void do_conf(const nlohmann::json& obj);
  void do_start(const nlohmann::json& obj);
  void do_stop(const nlohmann::json& obj);
  void do_scrap(const nlohmann::json& obj);

  //Threading
  dunedaq::utilities::WorkerThread thread_;//thread for creating and sending trigger records
  void do_work(std::atomic<bool>&);
  dunedaq::utilities::WorkerThread rcthread_;//thread for receiving tokens
  void do_receive(std::atomic<bool>&);

  //Configuration
  daqdataformats::run_number_t runNumber;
  int dataSize; //parameter which indicates the size of one fragment without the fragment header
  std::string m_hardware_map_file; //parameter containing the path to the hardwaremap used for creating trigger record
  int tokenCount; //parameter used to prevent overloading
  uint16_t dtypeToUse; //subdetector type
  uint16_t stypeToUse = 1; //subsystem type, 1 is default
  uint16_t ftypeToUse; //fragment type
  int elementCount; //number of fragments in trigger record

  //Connections
  std::chrono::milliseconds queueTimeout_;
  using tr_sender = iomanager::SenderConcept<std::unique_ptr<daqdataformats::TriggerRecord>>;
  std::shared_ptr<tr_sender> m_sender;
  using token_receiver = iomanager::ReceiverConcept<dfmessages::TriggerDecisionToken>;
  std::shared_ptr<token_receiver> inputQueue_;

  trsender::Conf cfg_;

  // Statistic counters
  std::atomic<int64_t> sentCount = {0};
  std::atomic<int64_t> triggerRecordCount = {0};
  std::atomic<int64_t> receivedToken = {0};
  std::atomic<int64_t> TrTokenDifference = {0};
};

} // namespace dfmodules
} // namespace dunedaq
#endif // DFMODULES_PLUGINS_TRSENDER_HPP_