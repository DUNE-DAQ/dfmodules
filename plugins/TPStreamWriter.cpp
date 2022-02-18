/**
 * @file TPStreamWriter.cpp TPStreamWriter class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "TPStreamWriter.hpp"
#include "dfmodules/TPBundleHandler.hpp"
#include "dfmodules/tpstreamwriter/Nljs.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "daqdataformats/Fragment.hpp"
#include "daqdataformats/Types.hpp"
#include "logging/Logging.hpp"
#include "rcif/cmd/Nljs.hpp"

#include "boost/date_time/posix_time/posix_time.hpp"

#include <chrono>
#include <sstream>
#include <string>
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
  m_tpset_source.reset(new source_t(appfwk::queue_inst(payload, "tpset_source")));
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
TPStreamWriter::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";
  tpstreamwriter::ConfParams conf_params = payload.get<tpstreamwriter::ConfParams>();
  m_max_file_size = conf_params.max_file_size_bytes;
  TLOG_DEBUG(TLVL_CONFIG) << get_name() << ": max file size is " << m_max_file_size << " bytes.";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
TPStreamWriter::do_start(const nlohmann::json& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  rcif::cmd::StartParams start_params = payload.get<rcif::cmd::StartParams>();
  m_run_number = start_params.run;
  m_thread.start_working_thread(get_name());
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
TPStreamWriter::do_stop(const nlohmann::json& /*payload*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  m_thread.stop_working_thread();
  TLOG() << get_name() << " successfully stopped for run number " << m_run_number;
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
TPStreamWriter::do_scrap(const data_t& /*payload*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";
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

  TPBundleHandler tp_bundle_handler(2500000, m_run_number, std::chrono::seconds(1));

  while (running_flag.load()) {
    trigger::TPSet tpset;
    try {
      m_tpset_source->pop(tpset, std::chrono::milliseconds(100));
      ++n_tpset_received;
    } catch (appfwk::QueueTimeoutExpired&) {
      continue;
    }

    TLOG_DEBUG(9) << "Number of TPs in TPSet is " << tpset.objects.size() << ", GeoID is " << tpset.origin
                  << ", seqno is " << tpset.seqno << ", start timestamp is " << tpset.start_time << ", run number is "
                  << m_run_number << ", slice id is " << (tpset.start_time / 2500000);

    tp_bundle_handler.add_tpset(std::move(tpset));

    std::vector<std::unique_ptr<daqdataformats::TriggerRecord>> list_of_timeslices = tp_bundle_handler.get_properly_aged_timeslices();
    for (auto& timeslice : list_of_timeslices) {
      TLOG() << "================================";
      TLOG() << timeslice->get_header_data();
      for (auto& frag : timeslice->get_fragments_ref()) {
        TLOG() << frag->get_header();
      }
    }
    
    if (first_timestamp == 0) {
      first_timestamp = tpset.start_time;
    }
    last_timestamp = tpset.start_time;
  } // while(true)

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
