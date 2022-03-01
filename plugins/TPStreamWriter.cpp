/**
 * @file TPStreamWriter.cpp TPStreamWriter class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "TPStreamWriter.hpp"
#include "dfmodules/DataStore.hpp"
#include "dfmodules/TPBundleHandler.hpp"
#include "dfmodules/hdf5datastore/Nljs.hpp"
#include "dfmodules/hdf5datastore/Structs.hpp"
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

  TPBundleHandler tp_bundle_handler(5000000, m_run_number, std::chrono::seconds(1));

  // create the DataStore
  hdf5datastore::PathParams params1;
  params1.detector_group_type = "TPC";
  params1.detector_group_name = "TPC";
  params1.region_name_prefix = "APA";
  params1.digits_for_region_number = 3;
  params1.element_name_prefix = "Link";
  params1.digits_for_element_number = 2;
  hdf5datastore::PathParamList param_list;
  param_list.push_back(params1);
  hdf5datastore::FileLayoutParams layout_params;
  layout_params.path_param_list = param_list;
  layout_params.trigger_record_name_prefix = "TimeSlice";

  std::string file_path(".");
  std::string file_prefix = "tpstream";

  hdf5datastore::ConfParams config_params;
  config_params.name = "tpstream_writer";
  config_params.directory_path = file_path;
  config_params.mode = "all-per-file";
  config_params.max_file_size_bytes = 1000000000;
  config_params.filename_parameters.overall_prefix = file_prefix;
  config_params.operational_environment = "swtest";
  config_params.file_layout_parameters = layout_params;
  hdf5datastore::data_t hdf5ds_json;
  hdf5datastore::to_json(hdf5ds_json, config_params);
  std::unique_ptr<DataStore> data_store_ptr;
  data_store_ptr = make_data_store(hdf5ds_json);

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
                  << m_run_number << ", slice id is " << (tpset.start_time / 5000000);

    tp_bundle_handler.add_tpset(std::move(tpset));

    std::vector<std::unique_ptr<daqdataformats::TimeSlice>> list_of_timeslices =
      tp_bundle_handler.get_properly_aged_timeslices();
    for (auto& timeslice_ptr : list_of_timeslices) {
      TLOG() << "================================";
      TLOG() << timeslice_ptr->get_header();
      for (auto& frag : timeslice_ptr->get_fragments_ref()) {
        TLOG() << frag->get_header();
      }

      std::vector<KeyedDataBlock> data_block_list;
      uint64_t bytes_in_data_blocks = 0; // NOLINT(build/unsigned)

      // First deal with the timeslice header
      daqdataformats::TimeSliceHeader ts_header = timeslice_ptr->get_header();
      size_t tsh_size = sizeof(ts_header);
      StorageKey tsh_key(
        ts_header.run_number, ts_header.timeslice_number, StorageKey::DataRecordGroupType::kTriggerRecordHeader, 1, 1);
      tsh_key.m_this_sequence_number = 0;
      tsh_key.m_max_sequence_number = 0;
      KeyedDataBlock tsh_block(tsh_key);
      tsh_block.m_unowned_data_start = &ts_header;
      ;
      tsh_block.m_data_size = tsh_size;
      bytes_in_data_blocks += tsh_block.m_data_size;
      data_block_list.emplace_back(std::move(tsh_block));

      // Loop over the fragments
      const auto& frag_vec = timeslice_ptr->get_fragments_ref();
      for (const auto& frag_ptr : frag_vec) {

        // add information about each Fragment to the list of data blocks to be stored
        StorageKey::DataRecordGroupType group_type = get_group_type(frag_ptr->get_element_id().system_type);
        StorageKey fragment_skey(frag_ptr->get_run_number(),
                                 timeslice_ptr->get_header().timeslice_number,
                                 group_type,
                                 frag_ptr->get_element_id().region_id,
                                 frag_ptr->get_element_id().element_id);
        fragment_skey.m_this_sequence_number = 0;
        fragment_skey.m_max_sequence_number = 0;
        KeyedDataBlock data_block(fragment_skey);
        data_block.m_unowned_data_start = frag_ptr->get_storage_location();
        data_block.m_data_size = frag_ptr->get_size();
        bytes_in_data_blocks += data_block.m_data_size;

        data_block_list.emplace_back(std::move(data_block));
      }

      // write the TSH and the fragments as a set of data blocks
      bool should_retry = true;
      size_t retry_wait_usec = 1000;
      do {
        should_retry = false;
        try {
          data_store_ptr->write(data_block_list);
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
  } // while(true)

  data_store_ptr.reset(); // explicit destruction

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
