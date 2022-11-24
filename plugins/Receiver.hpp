/**
 * @file Receiver.hpp
 *
 * Developer(s) of this DAQModule have yet to replace this line with a brief description of the DAQModule.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_RECEIVER_HPP_
#define DFMODULES_PLUGINS_RECEIVER_HPP_

#include "appfwk/DAQModule.hpp"
#include "iomanager/Receiver.hpp"
#include "utilities/WorkerThread.hpp"
#include "ers/Issue.hpp"
#include "daqdataformats/TriggerRecord.hpp"

#include <atomic>
#include <memory>
#include <limits>
#include <string>

namespace dunedaq::dfmodules {

class Receiver : public dunedaq::appfwk::DAQModule
{
public:
  explicit Receiver(const std::string& name);

  void init(const data_t&) override;

  void get_info(opmonlib::InfoCollector&, int /*level*/) override;

  Receiver(const Receiver&) = delete;
  Receiver& operator=(const Receiver&) = delete;
  Receiver(Receiver&&) = delete;
  Receiver& operator=(Receiver&&) = delete;

  ~Receiver() = default;

private:
  //Commands
void do_start(const nlohmann::json& obj);
void do_stop(const nlohmann::json& obj);

//Threading
dunedaq::utilities::WorkerThread thread_;
void do_work(std::atomic<bool>&);

std::chrono::milliseconds queueTimeout_;
std::shared_ptr<iomanager::ReceiverConcept<std::unique_ptr<daqdataformats::TriggerRecord>>> m_receiver;

// Statistic counters
std::atomic<int64_t> receivedCount {0};
};

} // namespace dunedaq::dfmodules

#endif // DFMODULES_PLUGINS_RECEIVER_HPP_
