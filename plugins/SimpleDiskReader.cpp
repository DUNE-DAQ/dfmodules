/**
 * @file SimpleDiskReader.cpp SimpleDiskReader class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "SimpleDiskReader.hpp"
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
#define TRACE_NAME "SimpleDiskReader" // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10    // NOLINT
#define TLVL_WORK_STEPS 15            // NOLINT

namespace dunedaq {
namespace ddpdemo {

SimpleDiskReader::SimpleDiskReader(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&SimpleDiskReader::do_work, this, std::placeholders::_1))
{
  register_command("configure", &SimpleDiskReader::do_configure);
  register_command("start", &SimpleDiskReader::do_start);
  register_command("stop", &SimpleDiskReader::do_stop);
  register_command("unconfigure", &SimpleDiskReader::do_unconfigure);
}

void
SimpleDiskReader::init( const data_t& )
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
SimpleDiskReader::do_configure(const data_t& args )
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_configure() method";
  key_eventID_ = args.value<size_t>("event_id", static_cast<size_t>(REASONABLE_DEFAULT_INTFRAGMENT));
  key_detectorID_ = args["detector_id"].get<std::string>();
  key_geoLocationID_ =
    args.value<size_t>("geolocation_id", static_cast<size_t>(REASONABLE_DEFAULT_INTFRAGMENT));
  sleepMsecWhileRunning_ = args.value<size_t>("sleep_msec_while_running",
                                                      static_cast<size_t>(REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING));

  filename_pattern_ = args["data_store_parameters"]["filename"].get<std::string>();

  // Initializing the HDF5 DataStore constructor
  // Creating empty HDF5 file
  dataReader_ = makeDataStore( args["data_store_parameters"] ) ; 

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_configure() method";
}

void
SimpleDiskReader::do_start(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  thread_.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
SimpleDiskReader::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  thread_.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
SimpleDiskReader::do_unconfigure(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_unconfigure() method";
  //  nIntsPerFakeEvent_ = REASONABLE_DEFAULT_INTSPERFAKEEVENT;
  sleepMsecWhileRunning_ = REASONABLE_DEFAULT_SLEEPMSECWHILERUNNING;
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_unconfigure() method";
}

/**
 * @brief Format a std::vector<int> to a stream
 * @param t ostream Instance
 * @param ints Vector to format
 * @return ostream Instance
 */
std::ostream&
operator<<(std::ostream& t, std::vector<int> ints)
{
  t << "{";
  bool first = true;
  for (auto& i : ints) {
    if (!first)
      t << ", ";
    first = false;
    t << i;
  }
  return t << "}";
}

void
SimpleDiskReader::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";

  // ensure that we have a valid dataReader instance
  if (dataReader_.get() == nullptr) {
    throw InvalidDataReaderError(ERS_HERE, get_name());
  }

  TLOG(TLVL_WORK_STEPS) << get_name() << ": reading fakeEvent  ";

  StorageKey dataKey(key_eventID_, key_detectorID_, key_geoLocationID_);

  TLOG(TLVL_WORK_STEPS) << get_name() << ": trying to read fragment " << key_eventID_ << ":" << key_detectorID_ << ":"
                        << key_geoLocationID_ << ", from file " << filename_pattern_;

  KeyedDataBlock dataBlock = dataReader_->read(dataKey);

  while (running_flag.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMsecWhileRunning_));
  }

  TLOG(TLVL_WORK_STEPS) << get_name() << ": End of do_work loop";
  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, read fragment of size " << dataBlock.data_size << " with eventID of "
           << dataBlock.data_key.getEventID() << " and geoLoc of " << dataBlock.data_key.getGeoLocation();
  ers::info(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace ddpdemo
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::ddpdemo::SimpleDiskReader)
