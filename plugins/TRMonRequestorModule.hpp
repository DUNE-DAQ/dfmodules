/**
 * @file TRMonRequestorModule.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_TRMONREQUESTORMODULE_HPP_
#define DFMODULES_PLUGINS_TRMONREQUESTORMODULE_HPP_

#include "appfwk/DAQModule.hpp"
#include "appmodel/TRMonRequestorConf.hpp"
#include "iomanager/IOManager.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TRMonRequest.hpp"
#include "utilities/WorkerThread.hpp"
#include "dfmodules/opmon/TRMonRequestorModule.pb.h"

#include <atomic>
#include <memory>
#include <chrono>
#include <string>
#include <map>

namespace dunedaq::dfmodules {
class TRMonRequestorModule : public appfwk::DAQModule
{
public:
  /**
   * @brief TRMonRequestorModule Constructor
   * @param name Instance name for this TRMonRequestorModule instance
   */
  explicit TRMonRequestorModule(const std::string& name);

  TRMonRequestorModule(const TRMonRequestorModule&) = delete; ///< TRMonRequestorModule is not copy-constructible
  TRMonRequestorModule& operator=(const TRMonRequestorModule&) =
    delete;                                                         ///< TRMonRequestorModule is not copy-assignable
  TRMonRequestorModule(TRMonRequestorModule&&) = delete;            ///< TRMonRequestorModule is not move-constructible
  TRMonRequestorModule& operator=(TRMonRequestorModule&&) = delete; ///< TRMonRequestorModule is not move-assignable

  void init(std::shared_ptr<appfwk::ConfigurationManager> mcfg) override;

  void generate_opmon_data() override;

protected:

  using token_receiver_t = iomanager::ReceiverConcept<dfmessages::TriggerDecisionToken>;
  using trmon_sender_t = iomanager::SenderConcept<dfmessages::TRMonRequest>;

  // Commands
  void do_conf(const CommandData_t&);
  void do_scrap(const CommandData_t&);
  void do_start(const CommandData_t&);
  void do_stop(const CommandData_t&);

  // Callbacks
  void token_callback(const dfmessages::TriggerDecisionToken&);

  // Working thread
  void do_work(std::atomic<bool>&);

  // Requests
  void send_trmon_request();

private:
  // Configuration
  std::chrono::milliseconds m_request_interval;
  std::string m_reply_connection;
  dfmessages::trigger_type_t m_trigger_type_mask{ 0 };
  dfmessages::request_number_t m_current_request_number{ 0 };
  const appmodel::TRMonRequestorConf* m_requestor_conf;
  utilities::WorkerThread m_working_thread;

  // Run information
  std::atomic<size_t> m_token_count{ 0 };
  std::unique_ptr<const daqdataformats::run_number_t> m_run_number = nullptr;

  // Communication
  std::map<std::string, std::shared_ptr<trmon_sender_t>> m_trmon_senders;
  std::map<std::string, std::shared_ptr<trmon_sender_t>>::const_iterator m_trmon_sender_iter;
  std::shared_ptr<token_receiver_t> m_token_receiver;

  // Monitoring
  using const_metric_counter_t = std::invoke_result<decltype(&dunedaq::dfmodules::opmon::TRMonRequestorInfo::trigger_records_requested),
						    dunedaq::dfmodules::opmon::TRMonRequestorInfo>::type;
  using metric_counter_t = std::remove_const<const_metric_counter_t>::type;
  std::atomic<metric_counter_t> m_trigger_records_requested{ 0 };
};
}

#endif // DFMODULES_PLUGINS_TRMONREQUESTORMODULE_HPP_
