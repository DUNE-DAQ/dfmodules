/**
 * @file TPStreamWriter.cpp TPStreamWriter class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "TPStreamWriter.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/TPBundleHandler.hpp"
#include "dfmodules/tpstreamwriter/Nljs.hpp"
#include "dfmodules/tpstreamwriterinfo/InfoNljs.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "iomanager/IOManager.hpp"
#include "daqdataformats/Fragment.hpp"
#include "daqdataformats/Types.hpp"
#include "logging/Logging.hpp"
#include "rcif/cmd/Nljs.hpp"

#include "boost/date_time/posix_time/posix_time.hpp"

#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_CONFIG = 7,
};

namespace dunedaq {
namespace dfmodules {

TPStreamWriter::TPStreamWriter(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_thread(std::bind(&TPStreamWriter::do_work, this, std::placeholders::_1))
  , m_queue_timeout(100)
{
  register_command("conf", &TPStreamWriter::do_conf);
  register_command("start", &TPStreamWriter::do_start);
  register_command("stop", &TPStreamWriter::do_stop);
  register_command("scrap", &TPStreamWriter::do_scrap);
}

void
TPStreamWriter::init(const nlohmann::json& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  m_tpset_source = iomanager::IOManager::get()->get_receiver<incoming_t>( appfwk::connection_uid(payload, "tpset_source"));
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
TPStreamWriter::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  tpstreamwriterinfo::Info info;

  info.tpset_received = m_tpset_received.exchange(0);
  info.tpset_written = m_tpset_written.exchange(0);
  info.bytes_output = m_bytes_output.exchange(0);

  ci.add(info);
}

void
TPStreamWriter::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";
  tpstreamwriter::ConfParams conf_params = payload.get<tpstreamwriter::ConfParams>();
  m_accumulation_interval_ticks = conf_params.tp_accumulation_interval_ticks;
  m_source_id = conf_params.source_id;
  m_fw_tpg_enabled = conf_params.firmware_tpg_enabled;

  // create the DataStore instance here
  try {
    m_data_writer = make_data_store(payload["data_store_parameters"]);
  } catch (const ers::Issue& excpt) {
    throw UnableToConfigure(ERS_HERE, get_name(), excpt);
  }

  // ensure that we have a valid dataWriter instance
  if (m_data_writer.get() == nullptr) {
    throw InvalidDataWriter(ERS_HERE, get_name());
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
TPStreamWriter::do_start(const nlohmann::json& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  rcif::cmd::StartParams start_params = payload.get<rcif::cmd::StartParams>();
  m_run_number = start_params.run;

  // 06-Mar-2022, KAB: added this call to allow DataStore to prepare for the run.
  // I've put this call fairly early in this method because it could throw an
  // exception and abort the run start.  And, it seems sensible to avoid starting
  // threads, etc. if we throw an exception.
  try {
    m_data_writer->prepare_for_run(m_run_number);
  } catch (const ers::Issue& excpt) {
    throw UnableToStart(ERS_HERE, get_name(), m_run_number, excpt);
  }

  m_thread.start_working_thread(get_name());

  TLOG() << get_name() << " successfully started for run number " << m_run_number;
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
TPStreamWriter::do_stop(const nlohmann::json& /*payload*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  m_thread.stop_working_thread();

  // 06-Mar-2022, KAB: added this call to allow DataStore to finish up with this run.
  // I've put this call fairly late in this method so that any draining of queues
  // (or whatever) can take place before we finalize things in the DataStore.
  try {
    m_data_writer->finish_with_run(m_run_number);
  } catch (const std::exception& excpt) {
    ers::error(ProblemDuringStop(ERS_HERE, get_name(), m_run_number, excpt));
  }

  TLOG() << get_name() << " successfully stopped for run number " << m_run_number;
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
TPStreamWriter::do_scrap(const data_t& /*payload*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  // clear/reset the DataStore instance here
  m_data_writer.reset();

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
TPStreamWriter::do_work(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";

  using namespace std::chrono;
  size_t n_tpset_received = 0;
  auto start_time = steady_clock::now();
  daqdataformats::timestamp_t first_timestamp = 0;
  daqdataformats::timestamp_t last_timestamp = 0;

  TPBundleHandler tp_bundle_handler(m_accumulation_interval_ticks, m_run_number, std::chrono::seconds(1), m_fw_tpg_enabled);

  while (running_flag.load()) {
    trigger::TPSet tpset;
    try {
      tpset = m_tpset_source->receive(m_queue_timeout);
      ++n_tpset_received;
      ++m_tpset_received;
    } catch (iomanager::TimeoutExpired&) {
      continue;
    }

    TLOG_DEBUG(21) << "Number of TPs in TPSet is " << tpset.objects.size() << ", Source ID is " << tpset.origin
                   << ", seqno is " << tpset.seqno << ", start timestamp is " << tpset.start_time << ", run number is "
                   << tpset.run_number << ", slice id is " << (tpset.start_time / m_accumulation_interval_ticks);

    // 30-Mar-2022, KAB: added test for matching run number.  This is to avoid getting
    // confused by TPSets that happen to be leftover in transit from one run to the
    // next (which we have observed in v2.10.x systems).
    if (tpset.run_number != m_run_number) {
      TLOG_DEBUG(22) << "Discarding TPSet with invalid run number " << tpset.run_number << " (current is "
                     << m_run_number << "),  Source ID is " << tpset.origin << ", seqno is " << tpset.seqno;
      continue;
    }

    tp_bundle_handler.add_tpset(std::move(tpset));

    std::vector<std::unique_ptr<daqdataformats::TimeSlice>> list_of_timeslices =
      tp_bundle_handler.get_properly_aged_timeslices();
    for (auto& timeslice_ptr : list_of_timeslices) {
      daqdataformats::SourceID sid(daqdataformats::SourceID::Subsystem::kTRBuilder, m_source_id);
      timeslice_ptr->set_element_id(sid);

      // write the TSH and the fragments as a set of data blocks
      bool should_retry = true;
      size_t retry_wait_usec = 1000;
      do {
        should_retry = false;
        try {
          m_data_writer->write(*timeslice_ptr);
	  ++m_tpset_written;
	  m_bytes_output += timeslice_ptr->get_total_size_bytes();
        } catch (const RetryableDataStoreProblem& excpt) {
          should_retry = true;
          ers::error(DataWritingProblem(ERS_HERE,
                                        get_name(),
                                        timeslice_ptr->get_header().timeslice_number,
                                        timeslice_ptr->get_header().run_number,
                                        excpt));
          if (retry_wait_usec > 1000000) {
            retry_wait_usec = 1000000;
          }
          usleep(retry_wait_usec);
          retry_wait_usec *= 2;
        } catch (const std::exception& excpt) {
          ers::error(DataWritingProblem(ERS_HERE,
                                        get_name(),
                                        timeslice_ptr->get_header().timeslice_number,
                                        timeslice_ptr->get_header().run_number,
                                        excpt));
        }
      } while (should_retry && running_flag.load());
    }

    if (first_timestamp == 0) {
      first_timestamp = tpset.start_time;
    }
    last_timestamp = tpset.start_time;
  } // while(running)

  auto end_time = steady_clock::now();
  auto time_ms = duration_cast<milliseconds>(end_time - start_time).count();
  float rate_hz = 1e3 * static_cast<float>(n_tpset_received) / time_ms;
  float inferred_clock_frequency = 1e3 * (last_timestamp - first_timestamp) / time_ms;

  TLOG() << "Received " << n_tpset_received << " TPSets in " << time_ms << "ms. " << rate_hz
         << " TPSet/s. Inferred clock frequency " << inferred_clock_frequency << "Hz";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
} // NOLINT Function length

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::TPStreamWriter)
