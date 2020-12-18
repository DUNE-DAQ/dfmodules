/**
 * @file DataTransferModule.cpp DataTransferModule class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "ddpdemo/datatransfermodule/Nljs.hpp"

#include "DataTransferModule.hpp"
#include "ddpdemo/DataStore.hpp"
#include "ddpdemo/KeyedDataBlock.hpp"

#include <TRACE/trace.h>
#include <ers/ers.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "DataTransferModule" // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10      // NOLINT
#define TLVL_WORK_STEPS 15              // NOLINT

namespace dunedaq {
namespace ddpdemo {

DataTransferModule::DataTransferModule(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&DataTransferModule::do_work, this, std::placeholders::_1))
{
  register_command("conf", &DataTransferModule::do_conf);
  register_command("start", &DataTransferModule::do_start);
  register_command("stop", &DataTransferModule::do_stop);
  register_command("unconfigure", &DataTransferModule::do_unconfigure);
}

void
DataTransferModule::init(const data_t&)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DataTransferModule::do_conf(const data_t& payload)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  datatransfermodule::Conf tmpConfig = payload.get<datatransfermodule::Conf>();

  sleepMsecWhileRunning_ = tmpConfig.sleep_msec_while_running;

  inputDataStore_ = makeDataStore(payload["input_data_store_parameters"]);

  outputDataStore_ = makeDataStore(payload["output_data_store_parameters"]);

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
DataTransferModule::do_start(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  thread_.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DataTransferModule::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  thread_.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DataTransferModule::do_unconfigure(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_unconfigure() method";
  sleepMsecWhileRunning_ = REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING;
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_unconfigure() method";
}

void
DataTransferModule::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";

  // ensure that we have a valid dataStore instances
  if (inputDataStore_.get() == nullptr) {
    throw InvalidDataStoreError(ERS_HERE, get_name(), "reading");
  }
  if (outputDataStore_.get() == nullptr) {
    throw InvalidDataStoreError(ERS_HERE, get_name(), "writing");
  }

  std::vector<StorageKey> keyList = inputDataStore_->getAllExistingKeys();
  for (auto& key : keyList) {
    KeyedDataBlock dataBlock = inputDataStore_->read(key);
    outputDataStore_->write(dataBlock);
  }

  while (running_flag.load()) {
    TLOG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep while waiting for run Stop";
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMsecWhileRunning_));
    TLOG(TLVL_WORK_STEPS) << get_name() << ": End of sleep while waiting for run Stop";
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, copied data for " << keyList.size() << " keys.";
  ers::info(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace ddpdemo
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::ddpdemo::DataTransferModule)
