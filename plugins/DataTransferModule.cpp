/**
 * @file DataTransferModule.cpp DataTransferModule class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/datatransfermodule/Nljs.hpp"

#include "DataTransferModule.hpp"
#include "dfmodules/DataStore.hpp"
#include "dfmodules/KeyedDataBlock.hpp"

//#include "TRACE/trace.h"
//#include "ers/ers.h"
#include "logging/Logging.hpp"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "DataTransferModule"        // NOLINT
enum {
	TLVL_ENTER_EXIT_METHODS=5,
	TLVL_WORK_STEPS=10
};

namespace dunedaq {
namespace dfmodules {

DataTransferModule::DataTransferModule(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_thread(std::bind(&DataTransferModule::do_work, this, std::placeholders::_1))
{
  register_command("conf", &DataTransferModule::do_conf);
  register_command("start", &DataTransferModule::do_start);
  register_command("stop", &DataTransferModule::do_stop);
  register_command("unconfigure", &DataTransferModule::do_unconfigure);
}

void
DataTransferModule::init(const data_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DataTransferModule::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  datatransfermodule::Conf tmpConfig = payload.get<datatransfermodule::Conf>();

  m_sleep_msec_wile_running = tmpConfig.sleep_msec_while_running;

  m_input_data_store = make_data_store(payload["input_data_store_parameters"]);

  m_output_data_store = make_data_store(payload["output_data_store_parameters"]);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
DataTransferModule::do_start(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  m_thread.start_working_thread();
  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DataTransferModule::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  m_thread.stop_working_thread();
  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DataTransferModule::do_unconfigure(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_unconfigure() method";
  m_sleep_msec_wile_running = REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING;
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_unconfigure() method";
}

void
DataTransferModule::do_work(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";

  // ensure that we have a valid dataStore instances
  if (m_input_data_store.get() == nullptr) {
    throw InvalidDataStoreError(ERS_HERE, get_name(), "reading");
  }
  if (m_output_data_store.get() == nullptr) {
    throw InvalidDataStoreError(ERS_HERE, get_name(), "writing");
  }

  std::vector<StorageKey> keyList = m_input_data_store->get_all_existing_keys();
  for (auto& key : keyList) {
    KeyedDataBlock data_block = m_input_data_store->read(key);
    m_output_data_store->write(data_block);
  }

  while (running_flag.load()) {
    TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep while waiting for run Stop";
    std::this_thread::sleep_for(std::chrono::milliseconds(m_sleep_msec_wile_running));
    TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": End of sleep while waiting for run Stop";
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, copied data for " << keyList.size() << " keys.";
  ers::log(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DataTransferModule)
