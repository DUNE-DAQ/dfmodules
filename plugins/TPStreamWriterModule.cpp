/**
 * @file TPStreamWriterModule.cpp TPStreamWriterModule class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "TPStreamWriterModule.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/TPBundleHandler.hpp"
#include "dfmodules/opmon/TPStreamWriter.pb.h"

#include "appmodel/DataStoreConf.hpp"
#include "appmodel/TPStreamWriterModule.hpp"
#include "confmodel/Connection.hpp"
#include "confmodel/Session.hpp"
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

TPStreamWriterModule::TPStreamWriterModule(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_thread(std::bind(&TPStreamWriterModule::do_work, this, std::placeholders::_1))
  , m_queue_timeout(100)
{
  register_command("conf", &TPStreamWriterModule::do_conf);
  register_command("start", &TPStreamWriterModule::do_start);
  register_command("stop", &TPStreamWriterModule::do_stop);
  register_command("scrap", &TPStreamWriterModule::do_scrap);
}

void
TPStreamWriterModule::init(std::shared_ptr<appfwk::ModuleConfiguration> mcfg)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto mdal = mcfg->module<appmodel::TPStreamWriterModule>(get_name());
  if (!mdal) {
    throw appfwk::CommandFailed(ERS_HERE, "init", get_name(), "Unable to retrieve configuration object");
  }
  assert(mdal->get_inputs().size() == 1);
  m_module_configuration = mcfg;
  m_tpset_source = iomanager::IOManager::get()->get_receiver<trigger::TPSet>(mdal->get_inputs()[0]->UID());
  m_tp_writer_conf = mdal->get_configuration();
  m_source_id = mdal->get_source_id();
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
TPStreamWriterModule::generate_opmon_data() {
  opmon::TPStreamWriterInfo info;

  info.set_tpset_received(m_tpset_received.exchange(0));
  info.set_tp_received( m_tp_received.exchange(0) );
  info.set_tp_written(m_tp_written.exchange(0));

  publish(std::move(info));
}

void
TPStreamWriterModule::do_conf(const data_t& )
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";
  m_accumulation_interval_ticks = m_tp_writer_conf->get_tp_accumulation_interval();

  // create the DataStore instance here
  try {
    m_data_writer = make_data_store(m_tp_writer_conf->get_data_store_params()->get_type(),
                                    m_tp_writer_conf->get_data_store_params()->UID(),
                                    m_module_configuration);
    register_node("data_writer", m_data_writer);
  } catch (const ers::Issue& excpt) {
    throw UnableToConfigure(ERS_HERE, get_name(), excpt);
  }

  // ensure that we have a valid dataWriter instance
  if (m_data_writer.get() == nullptr) {
    throw InvalidDataWriterModule(ERS_HERE, get_name());
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
TPStreamWriterModule::do_start(const nlohmann::json& payload)
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
TPStreamWriterModule::do_stop(const nlohmann::json& /*payload*/)
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
TPStreamWriterModule::do_scrap(const data_t& /*payload*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_scrap() method";

  // clear/reset the DataStore instance here
  m_data_writer.reset();

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_scrap() method";
}

void
TPStreamWriterModule::do_work(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";

  using namespace std::chrono;
  size_t n_tpset_received = 0;
  auto start_time = steady_clock::now();
  daqdataformats::timestamp_t first_timestamp = 0;
  daqdataformats::timestamp_t last_timestamp = 0;

  TPBundleHandler tp_bundle_handler(m_accumulation_interval_ticks, m_run_number, std::chrono::seconds(1));

  while (running_flag.load()) {
    trigger::TPSet tpset;
    try {
      tpset = m_tpset_source->receive(m_queue_timeout);
      ++n_tpset_received;
      ++m_tpset_received;
      m_tp_received += tpset.objects.size();
    } catch (iomanager::TimeoutExpired&) {
      continue;
    }

    if (tpset.type == trigger::TPSet::Type::kHeartbeat)
	    continue;

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
	  size_t n_tp = 0;
	  const auto & frags = timeslice_ptr -> get_fragments_ref();
	  for ( const auto & f_ptr :  frags ) {
	    n_tp += f_ptr -> get_data_size()/sizeof(trgdataformats::TriggerPrimitive);
	  }
	  m_tp_written += n_tp;
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

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::TPStreamWriterModule)
