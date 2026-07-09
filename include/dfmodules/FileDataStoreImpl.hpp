/**
 * @file FileDataStoreImpl.hpp
 *
 * An near-complete implementation of the DataStore interface which
 * nonetheless templatizes the representation of the data file (e.g.,
 * hdf5, Root, text, etc.) and leaves a few implementation
 * details (e.g., the physical closing and reopening of data files)
 * to a derived class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_INCLUDE_DFMODULES_FILEDATASTOREIMPL_HPP_
#define DFMODULES_INCLUDE_DFMODULES_FILEDATASTOREIMPL_HPP_

#include "dfmodules/DataStore.hpp"
#include "dfmodules/opmon/DataStore.pb.h"

#include "appmodel/FilenameParams.hpp"
#include "confmodel/DetectorConfig.hpp"
#include "confmodel/Session.hpp"

#include "appfwk/DAQModule.hpp"
#include "daqdataformats/Types.hpp"
#include "logging/Logging.hpp" // NOTE: if ISSUES ARE DECLARED BEFORE include logging/Logging.hpp, TLOG_DEBUG<<issue wont work.

#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/lexical_cast.hpp"

#include <concepts>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <sys/statvfs.h>
#include <utility>
#include <vector>

namespace dunedaq {

// Disable coverage checking LCOV_EXCL_START
/**
 * @brief Various ERS Issues for exceptional data store situations
 */
ERS_DECLARE_ISSUE_BASE(dfmodules,
                       FileDataStoreImplBadConfiguration,
                       appfwk::GeneralDAQModuleIssue,
                       "Construction of the FileDataStoreImpl base class failed due to faulty configuration",
                       ((std::string)name),
		       )
  
ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidOperationMode,
                       appfwk::GeneralDAQModuleIssue,
                       "Selected operation mode \"" << selected_operation
                                                    << "\" is NOT supported. Please update the configuration file.",
                       ((std::string)name),
                       ((std::string)selected_operation))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       FileOperationProblem,
                       appfwk::GeneralDAQModuleIssue,
                       "A problem was encountered when opening or closing file \"" << filename << "\"",
                       ((std::string)name),
                       ((std::string)filename))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidOutputPath,
                       appfwk::GeneralDAQModuleIssue,
                       "The specified output destination, \"" << output_path
                                                              << "\", is not a valid file system path on this server.",
                       ((std::string)name),
                       ((std::string)output_path))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InsufficientDiskSpace,
                       appfwk::GeneralDAQModuleIssue,
                       "There is insufficient free space on the disk associated with output file path \""
                         << path << "\". There are " << free_bytes << " bytes free, and the "
                         << "required minimum is " << needed_bytes << " bytes based on " << criteria << ".",
                       ((std::string)name),
                       ((std::string)path)((size_t)free_bytes)((size_t)needed_bytes)((std::string)criteria))

ERS_DECLARE_ISSUE_BASE(dfmodules,
		       TimeSliceAlreadyExists,
		       appfwk::GeneralDAQModuleIssue,
		       "The TimeSlice record for timeslice #" << timeslice_number << " already exists.",
		       ((std::string)name),
		       ((daqdataformats::timeslice_number_t)timeslice_number))

// Re-enable coverage checking LCOV_EXCL_STOP
namespace dfmodules {

template<typename T>
concept FileHandleConcept = requires(T file_handle,
                                     const T const_file_handle,
                                     const daqdataformats::TriggerRecord& tr,
                                     const daqdataformats::TimeSlice& ts)
{
  { file_handle.write(tr) } -> std::same_as<void>;
  { file_handle.write(ts) } -> std::same_as<void>;
  { file_handle.timeslice_already_exists(ts) } -> std::convertible_to<bool>;
  { const_file_handle.get_file_name() } -> std::convertible_to<std::string>;
  { const_file_handle.get_recorded_size() } -> std::convertible_to<size_t>;
  { const_file_handle.get_uncompressed_raw_data_size() } -> std::convertible_to<size_t>;
  { const_file_handle.get_total_file_size() } -> std::convertible_to<size_t>;
};

/**
 * @brief FileDataStoreImpl contains functionality you'd generally want in a data store irrespective of file type
 */
  template <FileHandleConcept FileHandleClass, // E.g., hdf5libs::HDF5RawDataFile
	    typename DataStoreConf, // E.g., appmodel::DataStoreConf
	    unsigned FileIOInfo> // E.g., HighFive::File::OpenOrCreate
class FileDataStoreImpl : public DataStore
{

public:
  enum
  {
    TLVL_BASIC = 2
  };

  static constexpr size_t s_unset_record_number { std::numeric_limits<size_t>::max() };

  /**
   * @brief FileDataStoreImpl Constructor
   * @param name, path, filename, operationMode
   *
   */
  explicit FileDataStoreImpl(std::string const& name,
                         std::shared_ptr<appfwk::ConfigurationManager> mcfg,
                         std::string const& writer_name)
    : DataStore(name)
    , m_file_handle {nullptr}
    , m_run_number {0}
    , m_file_index {0}
    , m_writer_identifier {writer_name}
    , m_config_params { mcfg ? mcfg->get_dal<DataStoreConf>(name) : nullptr }
    , m_session { mcfg ? mcfg->get_session() : nullptr }
    , m_compression_level { m_config_params ? m_config_params->get_compression_level() : static_cast<unsigned>(0) }
    , m_open_flags_of_open_file {0}
    , m_operational_environment { m_session ? m_session->get_detector_configuration()->get_op_env() : "unavailable" }
    , m_offline_data_stream { m_session ? m_session->get_detector_configuration()->get_offline_data_stream() : "unavailable" }
    , m_run_is_for_test_purposes { false }
    , m_basic_name_of_open_file {""}
    , m_recorded_size {0}
    , m_uncompressed_raw_data_size {0}
    , m_previous_file_size {0}
    , m_total_file_size {0}
    , m_current_record_number {s_unset_record_number}
    , m_new_bytes {0}
    , m_new_objects {0}
    , m_operation_mode { m_config_params ? m_config_params->get_mode() : "unavailable" }
    , m_path {m_config_params ?  m_config_params->get_directory_path() : "unavailable" }
    , m_max_file_size {m_config_params ?  m_config_params->get_max_file_size() : std::numeric_limits<size_t>::max() }
    , m_disable_unique_suffix { m_config_params ? m_config_params->get_disable_unique_filename_suffix() : false }
    , m_free_space_safety_factor_for_write {m_config_params ? m_config_params->get_free_space_safety_factor() : std::numeric_limits<float>::max() }
  {
    TLOG_DEBUG(TLVL_BASIC) << get_name();

    if (!m_config_params || !m_session) {
      throw FileDataStoreImplBadConfiguration(ERS_HERE, get_name());
    }

    if (m_operation_mode != "one-event-per-file"
        && m_operation_mode != "all-per-file") {

      throw InvalidOperationMode(ERS_HERE, get_name(), m_operation_mode);
    }

    if (m_free_space_safety_factor_for_write < 1.1) {
      m_free_space_safety_factor_for_write = 1.1;
    }

    // 05-Apr-2022, KAB: added warning message when the output destination
    // is not a valid directory.
    struct statvfs vfs_results;
    int retval = statvfs(m_path.c_str(), &vfs_results);
    if (retval != 0) {
      ers::warning(InvalidOutputPath(ERS_HERE, get_name(), m_path));
    }
  }

  virtual void open_new_file(const std::string& unique_filename) = 0;

  // Getter functions which can be used by the implementation of open_new_file

  std::string get_application_name() const noexcept {
     return m_writer_identifier;
  }

  unsigned get_compression_level() const noexcept {
    return m_compression_level;
  }

  const DataStoreConf& get_configuration() const noexcept {
    return *m_config_params;
  }

  auto& get_file_handle() {
    return m_file_handle;
  }

  size_t get_file_index() const noexcept {
    return m_file_index.load();
  }

  const std::string& get_offline_data_stream() const noexcept {
    return m_offline_data_stream;
  }
  
  unsigned get_open_flags() const noexcept {
    return m_open_flags_of_open_file;
  }

  const std::string& get_operational_environment() const noexcept {
    return m_operational_environment;
  }

  bool get_run_is_for_test_purposes() const noexcept {
    return m_run_is_for_test_purposes;
  }

  daqdataformats::run_number_t get_run_number() const noexcept {
    return m_run_number;
  }

  const confmodel::Session& get_session() const noexcept {
    return *m_session;
  }


  void throw_if_insufficient_space_for_object(size_t obj_size, const std::string& obj_name) {

    size_t current_free_space = get_free_space(m_path);

    if (current_free_space < (m_free_space_safety_factor_for_write * obj_size)) {
      std::ostringstream msg_oss;
      msg_oss << "a safety factor of " << m_free_space_safety_factor_for_write << " times the " << obj_name << " size";
      InsufficientDiskSpace issue(ERS_HERE,
                                  get_name(),
                                  m_path,
                                  current_free_space,
                                  (m_free_space_safety_factor_for_write * obj_size),
                                  msg_oss.str());
      assert(m_file_handle);
      std::string msg =
        "writing a " + obj_name + " to file" + m_file_handle->get_file_name();
      throw RetryableDataStoreProblem(ERS_HERE, get_name(), msg, issue);
    }
  }

  /**
   * @brief FileDataStoreImpl write() 
   * Method used to write TriggerRecords into the data
   * file. Operational mode defined in the configuration file.
   *
   */
  virtual void write(const daqdataformats::TriggerRecord& tr)
  {
    size_t tr_size = tr.get_total_size_bytes();

    throw_if_insufficient_space_for_object(tr_size, "trigger record");

    increment_file_index_if_needed(tr_size, tr.get_header_ref().get_trigger_number());

    m_current_record_number = tr.get_header_ref().get_trigger_number();

    std::string full_filename = get_file_name(tr.get_header_ref().get_run_number());

    try {
      open_file_if_needed(full_filename, FileIOInfo);
    } catch (std::exception const& excpt) {
      throw FileOperationProblem(ERS_HERE, get_name(), full_filename, excpt);
    } catch (...) { // NOLINT(runtime/exceptions)
      // NOLINT here because we *ARE* re-throwing the exception!
      throw FileOperationProblem(ERS_HERE, get_name(), full_filename);
    }

    // write the record
    m_file_handle->write(tr);
    m_recorded_size = m_file_handle->get_recorded_size();
    m_uncompressed_raw_data_size = m_file_handle->get_uncompressed_raw_data_size();
    m_total_file_size = m_file_handle->get_total_file_size();

    m_new_bytes += m_total_file_size - m_previous_file_size;
    ++m_new_objects;
    m_previous_file_size.store(m_total_file_size.load());
  }

  /**
   * @brief FileDataStoreImpl write()
   *
   * Method used to write TimeSlices into the data file. Operational
   * mode defined in the configuration file.
   *
   */
  virtual void write(const daqdataformats::TimeSlice& ts)
  {
    size_t ts_size = ts.get_total_size_bytes();
    throw_if_insufficient_space_for_object(ts_size, "time slice");

    increment_file_index_if_needed(ts_size, ts.get_header().timeslice_number);

    m_current_record_number = ts.get_header().timeslice_number;

    std::string full_filename = get_file_name(ts.get_header().run_number);

    try {
      open_file_if_needed(full_filename, FileIOInfo);
    } catch (std::exception const& excpt) {
      throw FileOperationProblem(ERS_HERE, get_name(), full_filename, excpt);
    } catch (...) { // NOLINT(runtime/exceptions)
      // NOLINT here because we *ARE* re-throwing the exception!
      throw FileOperationProblem(ERS_HERE, get_name(), full_filename);
    }

    // write the record
    try {

      if (m_file_handle->timeslice_already_exists(ts)) {
	throw TimeSliceAlreadyExists(ERS_HERE, get_name(), ts.get_header().timeslice_number);
      }

      m_file_handle->write(ts);
      m_recorded_size = m_file_handle->get_recorded_size();
      m_uncompressed_raw_data_size = m_file_handle->get_uncompressed_raw_data_size();
      m_total_file_size = m_file_handle->get_total_file_size();

    } catch (TimeSliceAlreadyExists const& excpt) {
      std::string msg = "writing a time slice to file " + m_file_handle->get_file_name();
      throw IgnorableDataStoreProblem(ERS_HERE, get_name(), msg, excpt);
    }

    m_new_bytes += m_total_file_size - m_previous_file_size;
    ++m_new_objects;
    m_previous_file_size.store(m_total_file_size.load());
  }

  /**
   * @brief Informs the FileDataStoreImpl that writes or reads of records
   * associated with the specified run number will soon be requested.
   * This allows the DataStore to test that the output file path is valid
   * and any other checks that are useful in advance of the first data
   * blocks being written or read.
   *
   * This method may throw an exception if it finds a problem.
   */
  void prepare_for_run(daqdataformats::run_number_t run_number,
                       bool run_is_for_test_purposes)
  {
    m_run_number = run_number;
    m_run_is_for_test_purposes = run_is_for_test_purposes;

    struct statvfs vfs_results;
    TLOG_DEBUG(TLVL_BASIC) << get_name() << ": Preparing to get the statvfs results for path: \"" << m_path << "\"";

    int retval = statvfs(m_path.c_str(), &vfs_results);
    TLOG_DEBUG(TLVL_BASIC) << get_name() << ": statvfs return code is " << retval;
    if (retval != 0) {
      throw InvalidOutputPath(ERS_HERE, get_name(), m_path);
    }

    size_t free_space = vfs_results.f_bsize * vfs_results.f_bavail;
    TLOG_DEBUG(TLVL_BASIC) << get_name() << ": Free space on disk with path \"" << m_path << "\" is " << free_space
                           << " bytes. This will be compared with the maximum size of a single file ("
                           << m_max_file_size << ") as a simple test to see if there is enough free space.";
    if (free_space < m_max_file_size) {
      throw InsufficientDiskSpace(
        ERS_HERE, get_name(), m_path, free_space, m_max_file_size, "the configured maximum size of a single file");
    }

    m_file_index = 0;
    m_recorded_size = 0;
    m_uncompressed_raw_data_size = 0;
    m_current_record_number = s_unset_record_number;
  }

  /**
   * @brief Informs the HD5DataStore that writes or reads of records
   * associated with the specified run number have finished, for now.
   * This allows the DataStore to close open files and do any other
   * cleanup or shutdown operations that are useful once the writes or
   * reads for a given run number have finished.
   */
  void finish_with_run(daqdataformats::run_number_t /*run_number*/)
  {
    if (m_file_handle.get() != nullptr) {
      std::string open_filename = m_file_handle->get_file_name();
      try {
        m_file_handle.reset();
        m_run_number = 0;
      } catch (std::exception const& excpt) {
        m_run_number = 0;
        throw FileOperationProblem(ERS_HERE, get_name(), open_filename, excpt);
      } catch (...) { // NOLINT(runtime/exceptions)
        m_run_number = 0;
        // NOLINT here because we *ARE* re-throwing the exception!
        throw FileOperationProblem(ERS_HERE, get_name(), open_filename);
      }
    }
  }

  FileDataStoreImpl(const FileDataStoreImpl&) = delete;
  FileDataStoreImpl& operator=(const FileDataStoreImpl&) = delete;
  FileDataStoreImpl(FileDataStoreImpl&&) = delete;
  FileDataStoreImpl& operator=(FileDataStoreImpl&&) = delete;
  
protected:
  void generate_opmon_data() override
  {

    opmon::FileDataStoreImplInfo info;

    info.set_new_bytes_output(m_new_bytes.exchange(0));
    info.set_new_written_object(m_new_objects.exchange(0));
    info.set_bytes_in_file(m_total_file_size.load());
    info.set_written_files(m_file_index.load());
    publish(std::move(info), { { "path", m_path } });
  }

private:
  std::unique_ptr<FileHandleClass> m_file_handle;

  daqdataformats::run_number_t m_run_number;

  // Total number of generated files
  std::atomic<size_t> m_file_index;
  const std::string m_writer_identifier;

  const DataStoreConf* m_config_params;

  const confmodel::Session* m_session;

  unsigned m_compression_level; 

  unsigned m_open_flags_of_open_file;

  const std::string m_operational_environment;
  const std::string m_offline_data_stream;
  bool m_run_is_for_test_purposes;

  std::string m_basic_name_of_open_file;
  
  // Size of data being written, excluding metadata
  std::atomic<size_t> m_recorded_size;

  // Theoretical, "uncompressed" size of data being written, excluding metadata
  std::atomic<size_t> m_uncompressed_raw_data_size;

  // Used for tracking the "delta" of the current write
  std::atomic<size_t> m_previous_file_size = 0;

  // Total size of the file, including raw data, metadata, and free space
  std::atomic<size_t> m_total_file_size;

  // Record number for the record that is currently being written out
  // This is only useful for long-readout windows, in which there may
  // be multiple calls to write()
  size_t m_current_record_number;

  // incremental written data
  std::atomic<uint64_t> m_new_bytes;
  std::atomic<uint64_t> m_new_objects;

  const std::string m_operation_mode;
  const std::string m_path;
  const size_t m_max_file_size;
  const bool m_disable_unique_suffix;
  float m_free_space_safety_factor_for_write;


  /**
   * @brief Translates the specified input parameters into the appropriate filename.
   */
  std::string get_file_name(daqdataformats::run_number_t run_number)
  {
    std::ostringstream work_oss;
    work_oss << m_config_params->get_directory_path();
    if (work_oss.str().length() > 0) {
      work_oss << "/";
    }
    work_oss << m_operational_environment + "_" + m_config_params->get_filename_params()->get_file_type_prefix();
    if (work_oss.str().length() > 0) {
      work_oss << "_";
    }

    work_oss << m_config_params->get_filename_params()->get_run_number_prefix();
    work_oss << std::setw(m_config_params->get_filename_params()->get_digits_for_run_number()) << std::setfill('0')
             << run_number;
    work_oss << "_";

    work_oss << m_config_params->get_filename_params()->get_file_index_prefix();
    work_oss << std::setw(m_config_params->get_filename_params()->get_digits_for_file_index()) << std::setfill('0')
             << m_file_index;

    work_oss << "_" << m_writer_identifier;
    work_oss << ".hdf5";
    return work_oss.str();
  }

  // Check if a new file should be opened for the record
  void increment_file_index_if_needed(size_t size_of_object_to_write, size_t object_record_number)
  {
    float compression_factor {1.0};

    if (m_compression_level != 0 && m_recorded_size != 0) {
      // Without compression, the uncompressed raw data size is approximately the total file size, so it
      // serves as an approximation of what would have been written without compression
      compression_factor = static_cast<float>(m_file_handle->get_uncompressed_raw_data_size()) / m_file_handle->get_total_file_size();
    }

    float size_of_next_write = size_of_object_to_write / compression_factor;

    if ((m_total_file_size + size_of_next_write) > m_max_file_size && m_recorded_size > 0) {
      ++m_file_index;
      m_recorded_size = 0;
      m_uncompressed_raw_data_size = 0;
      m_previous_file_size.store(0);
      return;
    }

    // JCF, 06-27-2026: TODO: probably need to reset m_recorded_size, etc., as is done right above
    if (m_operation_mode == "one-event-per-file" &&
	m_current_record_number != s_unset_record_number &&
	m_current_record_number != object_record_number) {
      ++m_file_index;
      return;
    }
  }

  void open_file_if_needed(const std::string& file_name, unsigned open_flags)
  {

    if (m_file_handle.get() == nullptr || m_basic_name_of_open_file.compare(file_name) ||
        m_open_flags_of_open_file != open_flags) {

      // close an existing open file
      if (m_file_handle.get() != nullptr) {
        std::string open_filename = m_file_handle->get_file_name();
        try {
          m_file_handle.reset();
        } catch (std::exception const& excpt) {
          throw FileOperationProblem(ERS_HERE, get_name(), open_filename, excpt);
        } catch (...) { // NOLINT(runtime/exceptions)
          // NOLINT here because we *ARE* re-throwing the exception!
          throw FileOperationProblem(ERS_HERE, get_name(), open_filename);
        }
      }

      // 04-Feb-2021, KAB: adding unique substrings to the filename
      // 05-Feb-2026, KAB: moved this block of code *after* the block that closes an
      // existing open file. When the two blocks were executed in the opposite order,
      // it was possible that the closing of the currently-open file could take a
      // non-trivial amount of time, and then the timestamp in the filename (determined
      // here) and the timestamp in the creation_timestamp HDF5 file Attribute
      // (determined inside the HDF5RawDataFile constructor) could disagree.
      std::string unique_filename = file_name;
      if (!m_disable_unique_suffix) {
	time_t now = time(0);
	std::string file_creation_timestamp = boost::posix_time::to_iso_string(boost::posix_time::from_time_t(now));
        // timestamp substring
        size_t ufn_len = unique_filename.length();
        if (ufn_len > 6) { // len GT 6 gives us some confidence that we have at least x.hdf5
          std::string timestamp_substring = "_" + file_creation_timestamp;
          TLOG_DEBUG(TLVL_BASIC) << get_name() << ": timestamp substring for filename: " << timestamp_substring;
          unique_filename.insert(ufn_len - 5, timestamp_substring);
        }
      }

      // opening file for the first time OR something changed in the name or the way of opening the file
      TLOG_DEBUG(TLVL_BASIC) << get_name() << ": going to open file " << unique_filename << " with open_flags "
                             << std::to_string(open_flags);
      m_basic_name_of_open_file = file_name;
      m_open_flags_of_open_file = open_flags;

      open_new_file(unique_filename);

    } else {
      TLOG_DEBUG(TLVL_BASIC) << get_name() << ": Pointer file to  " << m_basic_name_of_open_file
                             << " was already opened with open_flags " << std::to_string(m_open_flags_of_open_file);
    }
  }
  
  size_t get_free_space(const std::string& the_path)
  {
    struct statvfs vfs_results;
    int retval = statvfs(the_path.c_str(), &vfs_results);
    if (retval != 0) {
      return 0;
    }
    return vfs_results.f_bsize * vfs_results.f_bavail;
  }
};
  
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_INCLUDE_DFMODULES_FILEDATASTOREIMPL_HPP_
