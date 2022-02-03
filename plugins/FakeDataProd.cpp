/**
 * @file FakeDataProd.cpp FakeDataProd class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "FakeDataProd.hpp"
#include "dfmodules/CommonIssues.hpp"
#include "dfmodules/fakedataprod/Nljs.hpp"
#include "dfmodules/fakedataprodinfo/InfoNljs.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "dfmessages/Fragment_serialization.hpp"
#include "dfmessages/TimeSync.hpp"
#include "logging/Logging.hpp"
#include "networkmanager/NetworkManager.hpp"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "FakeDataProd" // NOLINT
enum
{
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10,
  TLVL_TIME_SYNCS = 12
};

namespace dunedaq {
namespace dfmodules {

FakeDataProd::FakeDataProd(const std::string& name)
  : dunedaq::appfwk::DAQModule(name)
  , m_thread(std::bind(&FakeDataProd::do_work, this, std::placeholders::_1))
  , m_timesync_thread(std::bind(&FakeDataProd::do_timesync, this, std::placeholders::_1))
  , m_queue_timeout(100)
  , m_run_number(0)
  , m_data_request_input_queue(nullptr)
{
  register_command("conf", &FakeDataProd::do_conf);
  register_command("start", &FakeDataProd::do_start);
  register_command("stop", &FakeDataProd::do_stop);

  m_pid_of_current_process = getpid();
}

void
FakeDataProd::init(const data_t& init_data)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering init() method";
  auto qi = appfwk::queue_index(init_data, { "data_request_input_queue" });

  try {
    m_data_request_input_queue.reset(new datareqsource_t(qi["data_request_input_queue"].inst));
  } catch (const ers::Issue& excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(), "data_request_input_queue", excpt);
  }
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting init() method";
}

void
FakeDataProd::do_conf(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_conf() method";

  fakedataprod::ConfParams tmpConfig = payload.get<fakedataprod::ConfParams>();
  m_geoid.system_type = daqdataformats::GeoID::string_to_system_type(tmpConfig.system_type);
  m_geoid.region_id = tmpConfig.apa_number;
  m_geoid.element_id = tmpConfig.link_number;
  m_time_tick_diff = tmpConfig.time_tick_diff;
  m_frame_size = tmpConfig.frame_size;
  m_response_delay = tmpConfig.response_delay;
  m_fragment_type = daqdataformats::string_to_fragment_type(tmpConfig.fragment_type);
  m_timesync_connection_name = tmpConfig.timesync_connection_name;
  m_timesync_topic_name = tmpConfig.timesync_topic_name;

  networkmanager::NetworkManager::get().start_publisher(m_timesync_connection_name);

  TLOG_DEBUG(TLVL_CONFIG) << get_name() << ": configured for link number " << m_geoid.element_id;

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_conf() method";
}

void
FakeDataProd::do_start(const data_t& payload)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_start() method";
  m_sent_fragments = 0;
  m_received_requests = 0;
  m_run_number = payload.value<dunedaq::daqdataformats::run_number_t>("run", 0);
  m_thread.start_working_thread();
  m_timesync_thread.start_working_thread();
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_start() method";
}

void
FakeDataProd::do_stop(const data_t& /*args*/)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_stop() method";
  m_thread.stop_working_thread();
  m_timesync_thread.stop_working_thread();
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_stop() method";
}

void
FakeDataProd::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  fakedataprodinfo::Info info;
  info.requests_received = m_received_requests;
  info.fragments_sent = m_sent_fragments;
  ci.add(info);
}

void
FakeDataProd::do_timesync(std::atomic<bool>& running_flag)
{
  int sent_count = 0;
  uint64_t msg_seqno = 0;
  while (running_flag.load()) {
    auto time_now = std::chrono::system_clock::now().time_since_epoch();
    uint64_t current_timestamp = // NOLINT (build/unsigned)
      std::chrono::duration_cast<std::chrono::nanoseconds>(time_now).count();
    auto timesyncmsg = dfmessages::TimeSync(current_timestamp);
    timesyncmsg.run_number = m_run_number;
    timesyncmsg.sequence_number = ++msg_seqno;
    timesyncmsg.source_pid = m_pid_of_current_process;
    TLOG_DEBUG(TLVL_TIME_SYNCS) << "New timesync: daq=" << timesyncmsg.daq_time
                                << " wall=" << timesyncmsg.system_time << " run=" << timesyncmsg.run_number
                                << " seqno=" << timesyncmsg.sequence_number << " pid=" << timesyncmsg.source_pid;
    try {
      auto serialised_timesync = dunedaq::serialization::serialize(timesyncmsg, dunedaq::serialization::kMsgPack);
      networkmanager::NetworkManager::get().send_to(m_timesync_connection_name,
                                                    static_cast<const void*>(serialised_timesync.data()),
                                                    serialised_timesync.size(),
                                                    std::chrono::milliseconds(500),
                                                    m_timesync_topic_name);
      ++sent_count;
    } catch (ers::Issue& excpt) {
      ers::warning(
        TimeSyncTransmissionFailed(ERS_HERE, get_name(), m_timesync_connection_name, m_timesync_topic_name, excpt));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  TLOG() << get_name() << ": sent " << sent_count << " TimeSync messages.";
}

void
FakeDataProd::do_work(std::atomic<bool>& running_flag)
{
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Entering do_work() method";
  int32_t receivedCount = 0;

  while (running_flag.load()) {
    dfmessages::DataRequest data_request;
    try {
      m_data_request_input_queue->pop(data_request, m_queue_timeout);
      m_received_requests++;
      ++receivedCount;
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      // it is perfectly reasonable that there might be no data in the queue
      // some fraction of the times that we check, so we just continue on and try again
      continue;
    }

    // num_frames_to_send = ⌈window_size / tick_diff⌉
    size_t num_frames_to_send = (data_request.request_information.window_end -
                                 data_request.request_information.window_begin + m_time_tick_diff - 1) /
                                m_time_tick_diff;
    size_t num_bytes_to_send = num_frames_to_send * m_frame_size;

    // We don't care about the content of the data, but the size should be correct
    std::unique_ptr<void, decltype(free)*> fake_data(malloc(num_bytes_to_send), free); 

    // This should really not happen
    if (fake_data == nullptr) {
      throw dunedaq::dfmodules::MemoryAllocationFailed(ERS_HERE, get_name(), num_bytes_to_send);
    }
    std::unique_ptr<daqdataformats::Fragment> data_fragment_ptr(
								new daqdataformats::Fragment(fake_data.get(), 
											     num_bytes_to_send));
    data_fragment_ptr->set_trigger_number(data_request.trigger_number);
    data_fragment_ptr->set_run_number(m_run_number);
    data_fragment_ptr->set_element_id(m_geoid);
    data_fragment_ptr->set_error_bits(0);
    data_fragment_ptr->set_type(m_fragment_type);
    data_fragment_ptr->set_trigger_timestamp(data_request.trigger_timestamp);
    data_fragment_ptr->set_window_begin(data_request.request_information.window_begin);
    data_fragment_ptr->set_window_end(data_request.request_information.window_end);
    data_fragment_ptr->set_sequence_number(data_request.sequence_number);

    if (m_response_delay > 0) {
      std::this_thread::sleep_for(std::chrono::nanoseconds(m_response_delay));
    }

    try {
      auto serialised_frag = dunedaq::serialization::serialize(data_fragment_ptr, dunedaq::serialization::kMsgPack);
      networkmanager::NetworkManager::get().send_to(data_request.data_destination,
                                                    static_cast<const void*>(serialised_frag.data()),
                                                    serialised_frag.size(),
                                                    std::chrono::milliseconds(1000));
    } catch (ers::Issue& e) {
      ers::warning(FragmentTransmissionFailed(ERS_HERE, get_name(), data_request.trigger_number, e));
    }

  }

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, received Fake trigger decision messages for " << receivedCount
           << " triggers.";
  TLOG() << ProgressUpdate(ERS_HERE, get_name(), oss_summ.str());
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS) << get_name() << ": Exiting do_work() method";
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::FakeDataProd)
