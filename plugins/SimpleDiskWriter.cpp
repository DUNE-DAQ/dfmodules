/**
 * @file SimpleDiskWriter.cpp SimpleDiskWriter class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "SimpleDiskWriter.hpp"
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
#define TRACE_NAME "SimpleDiskWriter" // NOLINT
#define TLVL_ENTER_EXIT_METHODS 10    // NOLINT
#define TLVL_WORK_STEPS 15            // NOLINT

namespace dunedaq {
namespace ddpdemo {

SimpleDiskWriter::SimpleDiskWriter(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , thread_(std::bind(&SimpleDiskWriter::do_work, this, std::placeholders::_1))
{
  register_command("configure", &SimpleDiskWriter::do_configure);
  register_command("start", &SimpleDiskWriter::do_start);
  register_command("stop", &SimpleDiskWriter::do_stop);
  register_command("unconfigure", &SimpleDiskWriter::do_unconfigure);
}

void
SimpleDiskWriter::init( const data_t& )
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
SimpleDiskWriter::do_configure( const data_t& args )
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_configure() method";
  nIntsPerFakeEvent_ =
    args.value<size_t>("number_ints_per_fake_event", static_cast<size_t>(REASONABLE_DEFAULT_INTSPERFAKEEVENT));
  waitBetweenSendsMsec_ =
    args.value<size_t>("wait_between_sends_msec", static_cast<size_t>(REASONABLE_DEFAULT_MSECBETWEENSENDS));

  // Initializing the HDF5 DataStore constructor
  // Creating empty HDF5 file
  dataWriter_ = makeDataStore( args["data_store_parameters"] ) ; 

  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_configure() method";
}

void
SimpleDiskWriter::do_start(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  thread_.start_working_thread();
  ERS_LOG(get_name() << " successfully started");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
SimpleDiskWriter::do_stop(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  thread_.stop_working_thread();
  ERS_LOG(get_name() << " successfully stopped");
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
SimpleDiskWriter::do_unconfigure(const data_t& /*args*/)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_unconfigure() method";
  nIntsPerFakeEvent_ = REASONABLE_DEFAULT_INTSPERFAKEEVENT;
  waitBetweenSendsMsec_ = REASONABLE_DEFAULT_MSECBETWEENSENDS;
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
SimpleDiskWriter::do_work(std::atomic<bool>& running_flag)
{
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  size_t generatedCount = 0;
  size_t writtenCount = 0;
  int fakeDataValue = 1;

  // ensure that we have a valid dataWriter instance
  if (dataWriter_.get() == nullptr) {
    throw InvalidDataWriterError(ERS_HERE, get_name());
  }

  while (running_flag.load()) {
    TLOG(TLVL_WORK_STEPS) << get_name() << ": Creating fakeEvent of length " << nIntsPerFakeEvent_;
    std::vector<int> theFakeEvent(nIntsPerFakeEvent_);

    TLOG(TLVL_WORK_STEPS) << get_name() << ": Start of fill loop";
    for (size_t idx = 0; idx < nIntsPerFakeEvent_; ++idx) {
      theFakeEvent[idx] = fakeDataValue;
      fakeDataValue++;
    }
    ++generatedCount;
    std::ostringstream oss_prog;
    oss_prog << "Generated fake event #" << generatedCount << " with contents " << theFakeEvent << " and size "
             << theFakeEvent.size() << ". ";
    ers::debug(ProgressUpdate(ERS_HERE, get_name(), oss_prog.str()));

    // Here is where we will eventually write the data out to disk
    StorageKey dataKey(generatedCount, "FELIX", 101);
    KeyedDataBlock dataBlock(dataKey);
    dataBlock.data_size = theFakeEvent.size() * sizeof(int);
    dataBlock.unowned_data_start = reinterpret_cast<void*>(&theFakeEvent[0]); // NOLINT
    TLOG(TLVL_WORK_STEPS) << get_name() << ": size of fake event number " << dataBlock.data_key.getEventID() << " is "
                          << dataBlock.data_size << " bytes.";
    dataWriter_->write(dataBlock);
    ++writtenCount;

    TLOG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep between sends";
    std::this_thread::sleep_for(std::chrono::milliseconds(waitBetweenSendsMsec_));
    TLOG(TLVL_WORK_STEPS) << get_name() << ": End of do_work loop";
  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, generated " << generatedCount << " fake events and successfully wrote "
           << writtenCount << " of them to disk. ";
  ers::info(ProgressUpdate(ERS_HERE, get_name(), oss_summ.str()));
  TLOG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace ddpdemo
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::ddpdemo::SimpleDiskWriter)
