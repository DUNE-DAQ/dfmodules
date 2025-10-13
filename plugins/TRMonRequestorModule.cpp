/**
 * @file TRMonRequestorModule.cpp TRMonRequestorModule class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "TRMonRequestorModule.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/opmon/TRMonRequestorModule.pb.h"

#include "appmodel/TRMonRequestorModule.hpp"
#include "confmodel/Connection.hpp"
#include "logging/Logging.hpp"

#include "iomanager/IOManager.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief TRACE debug levels used in this source file
 */
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_INIT = 8,
  TLVL_WORK_STEPS = 10,
  TLVL_BOOKKEEPING = 15,
  TLVL_DISPATCH_DATAREQ = 21,
  TLVL_FRAGMENT_RECEIVE = 22
};

namespace dunedaq::dfmodules {

TRMonRequestorModule::TRMonRequestorModule(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_working_thread(std::bind(&TRMonRequestorModule::do_work, this, std::placeholders::_1))
{

  register_command("conf", &TRMonRequestorModule::do_conf);
  register_command("scrap", &TRMonRequestorModule::do_scrap);
  register_command("start", &TRMonRequestorModule::do_start);
  register_command("stop", &TRMonRequestorModule::do_stop);
}

void
TRMonRequestorModule::init(std::shared_ptr<appfwk::ConfigurationManager> mcfg)
{

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";

  //--------------------------------
  // Get single queues
  //---------------------------------

  auto mdal = mcfg->get_dal<appmodel::TRMonRequestorModule>(get_name());
  if (!mdal) {
    throw appfwk::CommandFailed(ERS_HERE, "init", get_name(), "Unable to retrieve configuration object");
  }

  auto iom = iomanager::IOManager::get();
  for (auto con : mdal->get_inputs()) {
    if (con->get_data_type() == datatype_to_string<dfmessages::TriggerDecisionToken>()) {
      m_token_receiver = iom->get_receiver<dfmessages::TriggerDecisionToken>(con->UID());
    }
  }

  if (m_token_receiver == nullptr) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "TriggerDecisionToken Input queue");
  }

  for (auto con : mdal->get_outputs()) {
    if (con->get_data_type() == datatype_to_string<dfmessages::TRMonRequest>()) {
      m_trmon_senders[con->UID()] = iom->get_sender<dfmessages::TRMonRequest>(con->UID());
    }
  }

  m_reply_connection = mdal->get_trigger_record_destination()->UID();
  m_requestor_conf = mdal->get_configuration();

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
TRMonRequestorModule::generate_opmon_data()
{

  opmon::TRMonRequestorInfo i;

  // operation metrics
  i.set_trigger_records_requested(m_trigger_records_requested.exchange(0));

  publish(std::move(i));
}

void
TRMonRequestorModule::do_conf(const CommandData_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  m_request_interval = std::chrono::milliseconds(m_requestor_conf->get_minimum_request_interval_ms());
  m_trigger_type_mask = m_requestor_conf->get_trigger_type_mask();
  if (m_trigger_type_mask == 0) {
    m_trigger_type_mask = dfmessages::TRMonTriggerTypes::s_any_trigger_type;
  }
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
TRMonRequestorModule::do_scrap(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  TLOG() << get_name() << " successfully scrapped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
TRMonRequestorModule::do_start(const CommandData_t& args)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";

  // clean books from possible previous memory
  m_trigger_records_requested.store(0);
  m_current_request_number = 0;
  m_token_count = m_requestor_conf->get_maximum_outstanding_requests();

  // 19-Dec-2024, KAB: check that DataRequest senders are ready to send. This is done so
  // that the IOManager infrastructure fetches the necessary connection details from
  // the ConnectivityService at 'start' time, instead of the first time that the sender
  // is used to send a message.  This avoids delays in the sending of the first request in
  // the first data-taking run in a DAQ session. Such delays can lead to undesirable
  // system behavior like trigger inhibits.
  {
    for (auto& sender_pair : m_trmon_senders) {
      bool is_ready = sender_pair.second->is_ready_for_sending(std::chrono::milliseconds(100));
      TLOG_DEBUG(0) << "The TRMonRequest sender for " << sender_pair.first << " " << (is_ready ? "is" : "is not")
                    << " ready.";
    }
    m_trmon_sender_iter = m_trmon_senders.begin();
  }

  m_run_number.reset(new const daqdataformats::run_number_t(args.at("run").get<daqdataformats::run_number_t>()));

  m_token_receiver->add_callback(std::bind(&TRMonRequestorModule::token_callback, this, std::placeholders::_1));

  m_working_thread.start_working_thread(get_name());

  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
TRMonRequestorModule::do_stop(const CommandData_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";

  m_working_thread.stop_working_thread();

  m_token_receiver->remove_callback();
  m_token_count = 0;

  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
TRMonRequestorModule::token_callback(const dfmessages::TriggerDecisionToken& token)
{
  if (token.run_number == *m_run_number) {
    m_token_count++;
  }
}

void
TRMonRequestorModule::do_work(std::atomic<bool>& run_flag)
{
  auto last_trmon_send = std::chrono::steady_clock::now();
  auto short_sleep = m_request_interval / 100;
  auto long_sleep = m_request_interval / 10;
  while (run_flag) {
    if (m_token_count.load() > 0) {
      if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - last_trmon_send) >
          m_request_interval) {
        send_trmon_request();
        last_trmon_send = std::chrono::steady_clock::now();
        m_token_count--;
      } else {
        std::this_thread::sleep_for(long_sleep);
      }
    } else {
      std::this_thread::sleep_for(short_sleep);
    }
  }
}

void
TRMonRequestorModule::send_trmon_request()
{
  if (m_trmon_senders.size() == 0) {
    return;
  }

  dfmessages::TRMonRequest req;
  req.request_number = ++m_current_request_number;
  req.trigger_type_mask = m_trigger_type_mask;
  req.run_number = *m_run_number;
  req.data_destination = m_reply_connection;

  m_trmon_sender_iter->second->send(std::move(req), std::chrono::milliseconds(100));
  ++m_trmon_sender_iter;
  if (m_trmon_sender_iter == m_trmon_senders.end()) {
    m_trmon_sender_iter = m_trmon_senders.begin();
  }
  m_trigger_records_requested++;
}

} // namespace dunedaq::dfmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::TRMonRequestorModule)
