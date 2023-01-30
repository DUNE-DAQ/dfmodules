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
#include "ers/Issue.hpp"
#include "utilities/WorkerThread.hpp"
#include "dfmodules/trsender/Structs.hpp"
#include "daqdataformats/TriggerRecord.hpp"
#include "daqdataformats/SourceID.hpp"
#include "detdataformats/DetID.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"

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
  dunedaq::utilities::WorkerThread thread_;
  void do_work(std::atomic<bool>&);
  dunedaq::utilities::WorkerThread rcthread_;
  void do_receive(std::atomic<bool>&);

  //Configuration
  daqdataformats::run_number_t runNumber;
  int dataSize;
  std::string m_hardware_map_file;
  int tokenCount;
  uint16_t dtypeToUse;
  uint16_t stypeToUse = 1; //default
  uint16_t ftypeToUse;
  int elementCount;
  //daqdataformats::SourceID::Subsystem stypeToUse; 
  //detdataformats::DetID::Subdetector dtypeToUse;
  //daqdataformats::FragmentType ftypeToUse;


  std::chrono::milliseconds queueTimeout_;
  std::shared_ptr<iomanager::SenderConcept<std::unique_ptr<daqdataformats::TriggerRecord>>> m_sender;
  std::shared_ptr<iomanager::ReceiverConcept<dfmessages::TriggerDecisionToken>> inputQueue_;
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
