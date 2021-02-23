/**
 * @file DataGenerator.cpp DataGenerator class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DataGenerator.hpp"
#include "dfmodules/DataStore.hpp"
#include "dfmodules/KeyedDataBlock.hpp"
#include "dfmodules/datagenerator/Nljs.hpp"

#include "logging/Logging.hpp"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief TRACE debug levels used in this source file
 */
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_WORK_STEPS = 10
};

namespace dunedaq {
namespace dfmodules {

DataGenerator::DataGenerator(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_thread(std::bind(&DataGenerator::do_work, this, std::placeholders::_1))
  , m_run_number(0)
{
  register_command("conf", &DataGenerator::do_conf);
  register_command("start", &DataGenerator::do_start);
  register_command("stop", &DataGenerator::do_stop);
  register_command("unconfigure", &DataGenerator::do_unconfigure);
}

void
DataGenerator::init(const data_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
DataGenerator::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  datagenerator::Conf temp_config = payload.get<datagenerator::Conf>();
  TLOG() << "Testing Conf creation. io_size is " << temp_config.io_size << ", and directory_path is \""
         << temp_config.data_store_parameters.directory_path << "\"";

  m_geo_loc_count =
    payload.value<size_t>("geo_location_count", static_cast<size_t>(m_reasonable_default_geo_loc_count));
  m_io_size = payload.value<size_t>("io_size", static_cast<size_t>(m_reasonable_default_io_size_bytes));
  m_sleep_msec_while_running =
    payload.value<size_t>("sleep_msec_while_running", static_cast<size_t>(m_reasonable_default_sleep_msec));

  // Create the DataStore instance
  m_data_writer = make_data_store(payload["data_store_parameters"]);

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
DataGenerator::do_start(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  m_run_number = payload.value<dunedaq::dataformats::run_number_t>("run", 0);
  m_thread.start_working_thread();
  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
DataGenerator::do_stop(const data_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  m_thread.stop_working_thread();
  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
DataGenerator::do_unconfigure(const data_t&)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_unconfigure() method";
  m_geo_loc_count = m_reasonable_default_geo_loc_count;
  m_io_size = m_reasonable_default_io_size_bytes;
  m_sleep_msec_while_running = m_reasonable_default_sleep_msec;
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_unconfigure() method";
}

void
DataGenerator::do_work(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  size_t written_count = 0;

  // ensure that we have a valid dataWriter instance
  if (m_data_writer.get() == nullptr) {
    throw InvalidDataWriterError(ERS_HERE, get_name());
  }

  // create a memory buffer
  void* membuffer = malloc(m_io_size);
  memset(membuffer, 'X', m_io_size);

  TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Generating data ";

  int event_id = 1;
  while (running_flag.load()) {
    for (size_t geo_id = 0; geo_id < m_geo_loc_count; ++geo_id) {
      // AAA: Component ID is fixed, to be changed later
      StorageKey data_key(m_run_number, event_id, "TPC", 0, geo_id);
      KeyedDataBlock data_block(data_key);
      data_block.m_data_size = m_io_size;

      // Set the data_block pointer to the start of the constant memory buffer
      data_block.m_unowned_data_start = membuffer;

      m_data_writer->write(data_block);
      ++written_count;
    }
    ++event_id;

    TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Start of sleep between sends";
    std::this_thread::sleep_for(std::chrono::milliseconds(m_sleep_msec_while_running));
    TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": End of do_work loop";
  }
  free(membuffer);

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, wrote " << written_count << " fragments associated with "
           << (event_id - 1) << " fake events. ";
  TLOG() << ProgressUpdate(ERS_HERE, get_name(), oss_summ.str());
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DataGenerator)
