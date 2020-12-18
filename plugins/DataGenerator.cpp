/**
 * @file DataGenerator.cpp DataGenerator class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "ddpdemo/datagenerator/Nljs.hpp"

#include "DataGenerator.hpp"
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
#define TRACE_NAME "DataGenerator" // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10 // NOLINT
#define TLVL_WORK_STEPS 15         // NOLINT

namespace dunedaq {
namespace ddpdemo {

DataGenerator::DataGenerator(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&DataGenerator::do_work, this, std::placeholders::_1))
{
  register_command("conf", &DataGenerator::do_conf);
  register_command("start", &DataGenerator::do_start);
  register_command("stop", &DataGenerator::do_stop);
  register_command("unconfigure", &DataGenerator::do_unconfigure);
}

void
DataGenerator::init( const data_t& )
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DataGenerator::do_conf( const data_t& payload )
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  datagenerator::Conf tmpConfig = payload.get<datagenerator::Conf>();
  ERS_LOG("Testing Conf creation. io_size is " << tmpConfig.io_size << ", and directory_path is \"" << tmpConfig.data_store_parameters.directory_path << "\"");

  nGeoLoc_ = payload.value<size_t>("geo_location_count", static_cast<size_t>(REASONABLE_DEFAULT_GEOLOC));
  io_size_ = payload.value<size_t>("io_size", static_cast<size_t>(REASONABLE_IO_SIZE_BYTES));
  sleepMsecWhileRunning_ = payload.value<size_t>("sleep_msec_while_running",
                                                      static_cast<size_t>(REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING));
  
  // Create the HDF5DataStore instance
  dataWriter_ = makeDataStore( payload["data_store_parameters"] ) ; 
  
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
DataGenerator::do_start( const data_t& )
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  thread_.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DataGenerator::do_stop( const data_t& )
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  thread_.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DataGenerator::do_unconfigure( const data_t& )
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_unconfigure() method";
  nGeoLoc_ = REASONABLE_DEFAULT_GEOLOC;
  io_size_ = REASONABLE_IO_SIZE_BYTES;
  sleepMsecWhileRunning_ = REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING;
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_unconfigure() method";
}

void
DataGenerator::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  size_t writtenCount = 0;

  // ensure that we have a valid dataWriter instance
  if (dataWriter_.get() == nullptr) {
    throw InvalidDataWriterError(ERS_HERE, get_name());
  }

  // create a memory buffer
  void* membuffer = malloc(io_size_);
  memset(membuffer, 'X', io_size_);

  TLOG(TLVL_WORK_STEPS) << get_name() << ": Generating data ";

  int eventID = 1;
  while (running_flag.load()) {
    for (size_t geoID = 0; geoID < nGeoLoc_; ++geoID) {
      // AAA: Component ID is fixed, to be changed later
      StorageKey dataKey(eventID, "FELIX", geoID);
      KeyedDataBlock dataBlock(dataKey);
      dataBlock.data_size = io_size_;

      // Set the dataBlock pointer to the start of the constant memory buffer
      dataBlock.unowned_data_start = membuffer;

      dataWriter_->write(dataBlock);
      ++writtenCount;
    }
    ++eventID;

    TLOG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep between sends";
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMsecWhileRunning_));
    TLOG(TLVL_WORK_STEPS) << get_name() << ": End of do_work loop";
  }
  free(membuffer);

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, wrote " << writtenCount << " fragments associated with " << (eventID - 1)
           << " fake events. ";
  ers::info(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace ddpdemo
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::ddpdemo::DataGenerator)
