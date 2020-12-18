/**
 * @file GetAllKeysTest.cpp GetAllKeysTest class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "GetAllKeysTest.hpp"
#include "HDF5DataStore.hpp"
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
#define TRACE_NAME "GetAllKeysTest" // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10  // NOLINT
#define TLVL_WORK_STEPS 15          // NOLINT

namespace dunedaq {
namespace ddpdemo {

GetAllKeysTest::GetAllKeysTest(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&GetAllKeysTest::do_work, this, std::placeholders::_1))
{
  register_command("configure", &GetAllKeysTest::do_configure);
  register_command("start", &GetAllKeysTest::do_start);
  register_command("stop", &GetAllKeysTest::do_stop);
  register_command("unconfigure", &GetAllKeysTest::do_unconfigure);
}

void
GetAllKeysTest::init( const data_t& )
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
GetAllKeysTest::do_configure(const data_t& args )
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_configure() method";
  sleepMsecWhileRunning_ = args.value<size_t>("sleep_msec_while_running",
                                                      static_cast<size_t>(REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING));

  dataStore_ = makeDataStore( args["data_store_parameters"] ) ; 

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_configure() method";
}

void
GetAllKeysTest::do_start(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  thread_.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
GetAllKeysTest::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  thread_.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
GetAllKeysTest::do_unconfigure(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_unconfigure() method";
  sleepMsecWhileRunning_ = REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING;
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_unconfigure() method";
}

void
GetAllKeysTest::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";

  // if we have a valid dataStore instance, fetch the set of existing keys
  if (dataStore_.get() != nullptr) {
    std::vector<StorageKey> keyVec = dataStore_->getAllExistingKeys();
    ERS_LOG(get_name() << ": StorageKey list has " << keyVec.size() << " elements.");
  }

  while (running_flag.load()) {
    TLOG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep while waiting for run Stop";
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMsecWhileRunning_));
    TLOG(TLVL_WORK_STEPS) << get_name() << ": End of sleep while waiting for run Stop";
  }

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace ddpdemo
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::ddpdemo::GetAllKeysTest)
