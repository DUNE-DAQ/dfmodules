/**
 * @file FileDataStore.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/FileDataStore.hpp"
#include "dfmodules/RawDataFileConfig.hpp"
#include "dfmodules/opmon/DataStore.pb.h"

#include "rawdatafilelibs/FileUtils.hpp"

#include "appmodel/FilenameParams.hpp"
#include "confmodel/DetectorConfig.hpp"
#include "daqdataformats/RecordID.hpp"

#include <chrono>
#include <ctime>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

namespace dunedaq::dfmodules {

namespace {
uint64_t // NOLINT(build/unsigned)
now_ms()
{
  return static_cast<uint64_t>( // NOLINT(build/unsigned)
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}
} // namespace

FileDataStore::FileDataStore(std::string const& name,
                             std::shared_ptr<appfwk::ConfigurationManager> mcfg,
                             std::string const& writer_name)
  : DataStore(name)
  , m_writer_identifier{ writer_name }
  , m_config_params{ mcfg ? mcfg->get_dal<appmodel::DataStoreConf>(name) : nullptr }
  , m_session{ mcfg ? mcfg->get_session() : nullptr }
  , m_source_id_geo_id_map{ m_session ? make_source_id_geo_id_map(*m_session) : daqdataformats::SourceIDGeoIDMap{} }
  , m_compression_level{ m_config_params ? m_config_params->get_compression_level() : 0U }
  , m_operational_environment{ m_session ? m_session->get_detector_configuration()->get_op_env() : "unavailable" }
  , m_offline_data_stream{ m_session ? m_session->get_detector_configuration()->get_offline_data_stream()
                                     : "unavailable" }
  , m_operation_mode{ m_config_params ? m_config_params->get_mode() : "unavailable" }
  , m_path{ m_config_params ? m_config_params->get_directory_path() : "unavailable" }
  , m_max_file_size{ m_config_params ? m_config_params->get_max_file_size() : std::numeric_limits<size_t>::max() }
  , m_disable_unique_suffix{ m_config_params ? m_config_params->get_disable_unique_filename_suffix() : false }
  , m_free_space_safety_factor_for_write{ m_config_params
                                            ? static_cast<float>(m_config_params->get_free_space_safety_factor())
                                            : std::numeric_limits<float>::max() }
{
  TLOG_DEBUG(TLVL_BASIC) << get_name();

  if (!m_config_params || !m_session) {
    throw FileDataStoreBadConfiguration(ERS_HERE, get_name());
  }

  if (m_operation_mode != "one-event-per-file" && m_operation_mode != "all-per-file") {
    throw InvalidOperationMode(ERS_HERE, get_name(), m_operation_mode);
  }

  // 05-Apr-2022, KAB: added warning message when the output destination
  // is not a valid directory.
  if (!rawdatafilelibs::get_free_space(m_path).has_value()) {
    ers::warning(InvalidOutputPath(ERS_HERE, get_name(), m_path));
  }
}

FileDataStore::~FileDataStore()
{
  try {
    close_file_if_open();
  } catch (ers::Issue const& issue) {
    ers::error(issue);
  }
}

void
FileDataStore::write(const daqdataformats::TriggerRecord& tr)
{
  auto const& header = tr.get_header_ref();
  write_record(tr,
               rawdatafilelibs::RecordType::kTriggerRecord,
               tr.get_total_size_bytes(),
               header.get_trigger_number(),
               header.get_run_number(),
               "trigger record");
}

void
FileDataStore::write(const daqdataformats::TimeSlice& ts)
{
  auto const header = ts.get_header();
  write_record(ts,
               rawdatafilelibs::RecordType::kTimeSlice,
               ts.get_total_size_bytes(),
               header.timeslice_number,
               header.run_number,
               "time slice");
}

template<typename RecordType>
void
FileDataStore::write_record(const RecordType& record,
                            rawdatafilelibs::RecordType record_type,
                            size_t record_size,
                            size_t record_number,
                            daqdataformats::run_number_t run_number,
                            const std::string& object_name)
{
  throw_if_insufficient_space_for_object(record_size, object_name);

  increment_file_index_if_needed(record_size, record_number);
  m_current_record_number = record_number;

  open_file_if_needed(get_file_name(run_number), record_type);

  if constexpr (std::is_same_v<RecordType, daqdataformats::TimeSlice>) {
    if (m_writer->record_exists(daqdataformats::RecordID{ record.get_header().timeslice_number })) {
      TimeSliceAlreadyExists excpt(ERS_HERE, get_name(), record.get_header().timeslice_number);
      throw IgnorableDataStoreProblem(
        ERS_HERE, get_name(), "writing a time slice to file " + m_writer->get_current_file_name(), excpt);
    }
  }

  m_writer->write(record);

  auto const stats = m_writer->get_stats();
  m_recorded_size = stats.recorded_size;
  m_uncompressed_raw_data_size = stats.uncompressed_raw_data_size;
  m_total_file_size = stats.total_file_size;

  m_new_bytes += m_total_file_size - m_previous_file_size;
  ++m_new_objects;
  m_previous_file_size.store(m_total_file_size.load());
}

void
FileDataStore::prepare_for_run(daqdataformats::run_number_t run_number, bool run_is_for_test_purposes)
{
  m_run_number = run_number;
  m_run_is_for_test_purposes = run_is_for_test_purposes;

  TLOG_DEBUG(TLVL_BASIC) << get_name() << ": Checking the free space for path: \"" << m_path << "\"";
  auto free_space = rawdatafilelibs::get_free_space(m_path);
  if (!free_space.has_value()) {
    throw InvalidOutputPath(ERS_HERE, get_name(), m_path);
  }

  TLOG_DEBUG(TLVL_BASIC) << get_name() << ": Free space on disk with path \"" << m_path << "\" is " << *free_space
                         << " bytes. This will be compared with the maximum size of a single file (" << m_max_file_size
                         << ") as a simple test to see if there is enough free space.";
  if (*free_space < m_max_file_size) {
    throw InsufficientDiskSpace(
      ERS_HERE, get_name(), m_path, *free_space, m_max_file_size, "the configured maximum size of a single file");
  }

  m_file_index = 0;
  reset_per_file_counters();
  m_current_record_number = s_unset_record_number;
}

void
FileDataStore::finish_with_run(daqdataformats::run_number_t /*run_number*/)
{
  m_run_number = 0;
  close_file_if_open();
}

void
FileDataStore::generate_opmon_data()
{
  opmon::FileDataStoreInfo info;

  info.set_new_bytes_output(m_new_bytes.exchange(0));
  info.set_new_written_object(m_new_objects.exchange(0));
  info.set_bytes_in_file(m_total_file_size.load());
  info.set_written_files(m_file_index.load());
  publish(std::move(info), { { "path", m_path } });
}

std::string
FileDataStore::get_file_name(daqdataformats::run_number_t run_number) const
{
  auto const* filename_conf = m_config_params->get_filename_params();
  rawdatafilelibs::FileNameParams params;
  params.directory_path = m_path;
  params.operational_environment = m_operational_environment;
  params.file_type_prefix = filename_conf->get_file_type_prefix();
  params.run_number_prefix = filename_conf->get_run_number_prefix();
  params.digits_for_run_number = filename_conf->get_digits_for_run_number();
  params.file_index_prefix = filename_conf->get_file_index_prefix();
  params.digits_for_file_index = filename_conf->get_digits_for_file_index();
  params.writer_identifier = m_writer_identifier;
  params.extension = get_file_name_extension();
  return rawdatafilelibs::make_file_name(params, run_number, m_file_index.load());
}

void
FileDataStore::throw_if_insufficient_space_for_object(size_t obj_size, const std::string& obj_name) const
{
  size_t current_free_space = rawdatafilelibs::get_free_space(m_path).value_or(0);
  auto needed_space = static_cast<size_t>(m_free_space_safety_factor_for_write * static_cast<float>(obj_size));

  if (current_free_space < needed_space) {
    std::ostringstream msg_oss;
    msg_oss << "a safety factor of " << m_free_space_safety_factor_for_write << " times the " << obj_name << " size";
    InsufficientDiskSpace issue(ERS_HERE, get_name(), m_path, current_free_space, needed_space, msg_oss.str());
    std::string msg = "writing a " + obj_name + " to file" + (m_writer ? " " + m_writer->get_current_file_name() : "");
    throw RetryableDataStoreProblem(ERS_HERE, get_name(), msg, issue);
  }
}

void
FileDataStore::increment_file_index_if_needed(size_t size_of_object_to_write, size_t object_record_number)
{
  float compression_factor{ 1.0 };

  if (m_compression_level != 0 && m_recorded_size != 0 && m_total_file_size != 0) {
    // Without compression, the uncompressed raw data size is approximately the total file size, so it
    // serves as an approximation of what would have been written without compression
    compression_factor = static_cast<float>(m_uncompressed_raw_data_size) / static_cast<float>(m_total_file_size);
  }

  float size_of_next_write = static_cast<float>(size_of_object_to_write) / compression_factor;

  if ((static_cast<float>(m_total_file_size) + size_of_next_write) > static_cast<float>(m_max_file_size) &&
      m_recorded_size > 0) {
    ++m_file_index;
    reset_per_file_counters();
    return;
  }

  if (m_operation_mode == "one-event-per-file" && m_current_record_number != s_unset_record_number &&
      m_current_record_number != object_record_number) {
    ++m_file_index;
    reset_per_file_counters();
  }
}

void
FileDataStore::reset_per_file_counters()
{
  m_recorded_size = 0;
  m_uncompressed_raw_data_size = 0;
  m_previous_file_size = 0;
  m_total_file_size = 0;
}

void
FileDataStore::close_file_if_open()
{
  if (!m_writer) {
    return;
  }
  auto writer = std::move(m_writer); // released even when closing fails, so a failure is reported once
  try {
    writer->close();
  } catch (std::exception const& excpt) {
    throw FileOperationProblem(ERS_HERE, get_name(), writer->get_current_file_name(), excpt);
  }
}

void
FileDataStore::open_file_if_needed(const std::string& file_name, rawdatafilelibs::RecordType record_type)
{
  if (m_writer && m_basic_name_of_open_file == file_name) {
    return;
  }

  close_file_if_open();

  // The creation time is taken once, after closing the previous file (which can take a while):
  // the file name (in seconds) and the file's creation timestamp come from the same reading.
  rawdatafilelibs::FileMetadata metadata;
  metadata.creation_timestamp_ms = now_ms();

  std::string unique_filename = file_name;
  if (!m_disable_unique_suffix) {
    unique_filename = rawdatafilelibs::add_timestamp_suffix(
      file_name, get_file_name_extension(), static_cast<std::time_t>(metadata.creation_timestamp_ms / 1000));
  }

  metadata.run_number = m_run_number;
  metadata.file_index = m_file_index.load();
  metadata.application_name = m_writer_identifier;
  metadata.record_type = record_type;
  metadata.operational_environment = m_operational_environment;
  metadata.offline_data_stream = m_offline_data_stream;
  metadata.run_is_for_test_purposes = m_run_is_for_test_purposes;
  metadata.compression_level = m_compression_level;

  TLOG_DEBUG(TLVL_BASIC) << get_name() << ": going to open file " << unique_filename;
  m_basic_name_of_open_file = file_name;

  try {
    // the writer creates the file under its in-progress name, and never reuses an existing file
    m_writer = std::make_unique<rawdatafilelibs::RawDataFileWriter>(
      make_writer_backend(record_type), unique_filename, metadata, m_source_id_geo_id_map);
  } catch (std::exception const& excpt) {
    throw FileOperationProblem(ERS_HERE, get_name(), unique_filename, excpt);
  }
  TLOG_DEBUG(TLVL_BASIC) << get_name() << ": created file " << m_writer->get_current_file_name();
}

} // namespace dunedaq::dfmodules
