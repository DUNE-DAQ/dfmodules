/**
 * @file FileDataStore.hpp
 *
 * The base class of DataStores that write raw data files, whatever their format.
 *
 * FileDataStore does everything that doesn't depend on the file format: choosing file names,
 * starting new files (size limit, one-event-per-file mode), checking free disk space, the file
 * metadata, duplicate-TimeSlice checks, error reporting and operational monitoring. Files are
 * written with rawdatafilelibs::RawDataFileWriter, which also handles the in-progress
 * (".writing") name and never reuses or overwrites an existing file.
 *
 * A DataStore for a format derives from FileDataStore and provides the file name extension and
 * a rawdatafilelibs::WriterBackend for each new file (see HDF5DataStore). Format-specific
 * configuration goes in an OKS class derived from appmodel::DataStoreConf (e.g.
 * appmodel::HDF5DataStoreConf), which the plugin can get with get_configuration().cast<>().
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_INCLUDE_DFMODULES_FILEDATASTORE_HPP_
#define DFMODULES_INCLUDE_DFMODULES_FILEDATASTORE_HPP_

#include "dfmodules/DataStore.hpp"

#include "rawdatafilelibs/Backend.hpp"
#include "rawdatafilelibs/FileMetadata.hpp"
#include "rawdatafilelibs/RawDataFileWriter.hpp"

#include "appfwk/DAQModule.hpp"
#include "appmodel/DataStoreConf.hpp"
#include "confmodel/Session.hpp"
#include "daqdataformats/SourceIDGeoIDMap.hpp"
#include "daqdataformats/TimeSlice.hpp"
#include "daqdataformats/TriggerRecord.hpp"
#include "daqdataformats/Types.hpp"
#include "logging/Logging.hpp" // NOTE: if ISSUES ARE DECLARED BEFORE include logging/Logging.hpp, TLOG_DEBUG<<issue wont work.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

namespace dunedaq {

// Disable coverage checking LCOV_EXCL_START
/**
 * @brief Various ERS Issues for exceptional data store situations
 */
ERS_DECLARE_ISSUE_BASE(dfmodules,
                       FileDataStoreBadConfiguration,
                       appfwk::GeneralDAQModuleIssue,
                       "Construction of the FileDataStore base class failed due to faulty configuration",
                       ((std::string)name), )

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

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       DeprecatedDataStoreConfiguration,
                       appfwk::GeneralDAQModuleIssue,
                       "Deprecated configuration: " << message,
                       ((std::string)name),
                       ((std::string)message))
// Re-enable coverage checking LCOV_EXCL_STOP

namespace dfmodules {

class FileDataStore : public DataStore
{
public:
  enum
  {
    TLVL_BASIC = 2
  };

  static constexpr size_t s_unset_record_number{ std::numeric_limits<size_t>::max() };

  // Throws FileDataStoreBadConfiguration, InvalidOperationMode
  FileDataStore(std::string const& name,
                std::shared_ptr<appfwk::ConfigurationManager> mcfg,
                std::string const& writer_name);

  // Closes any open file; failures are reported with ers::error
  ~FileDataStore() override;

  FileDataStore(const FileDataStore&) = delete;
  FileDataStore& operator=(const FileDataStore&) = delete;
  FileDataStore(FileDataStore&&) = delete;
  FileDataStore& operator=(FileDataStore&&) = delete;

  // Throw RetryableDataStoreProblem (e.g. too little disk space) or FileOperationProblem;
  // writing a TimeSlice that is already in the file throws IgnorableDataStoreProblem
  void write(const daqdataformats::TriggerRecord& tr) override;
  void write(const daqdataformats::TimeSlice& ts) override;

  // Throw InvalidOutputPath, InsufficientDiskSpace
  void prepare_for_run(daqdataformats::run_number_t run_number, bool run_is_for_test_purposes) override;

  // Closes the open file; throws FileOperationProblem if that fails
  void finish_with_run(daqdataformats::run_number_t run_number) override;

protected:
  // ---- what a format's DataStore provides ----

  // The file name extension, without the "."
  virtual std::string get_file_name_extension() const = 0;

  // A new, unopened backend for the next file, which will hold records of the given type
  virtual std::unique_ptr<rawdatafilelibs::WriterBackend> make_writer_backend(
    rawdatafilelibs::RecordType record_type) = 0;

  // ---- for derived classes ----

  const appmodel::DataStoreConf& get_configuration() const noexcept { return *m_config_params; }
  const confmodel::Session& get_session() const noexcept { return *m_session; }
  const daqdataformats::SourceIDGeoIDMap& get_source_id_geo_id_map() const noexcept { return m_source_id_geo_id_map; }

  void generate_opmon_data() override;

private:
  template<typename RecordType>
  void write_record(const RecordType& record,
                    rawdatafilelibs::RecordType record_type,
                    size_t record_size,
                    size_t record_number,
                    daqdataformats::run_number_t run_number,
                    const std::string& object_name);

  // The file name for the current run and file index, before any unique suffix
  std::string get_file_name(daqdataformats::run_number_t run_number) const;

  // Throws a RetryableDataStoreProblem if (size of the object)*(free space safety factor) exceeds the free space
  void throw_if_insufficient_space_for_object(size_t obj_size, const std::string& obj_name) const;

  // Moves to the next file index if the record doesn't fit in the open file, or in one-event-per-file mode
  void increment_file_index_if_needed(size_t size_of_object_to_write, size_t object_record_number);

  void reset_per_file_counters();
  void open_file_if_needed(const std::string& file_name, rawdatafilelibs::RecordType record_type);
  void close_file_if_open();

  std::unique_ptr<rawdatafilelibs::RawDataFileWriter> m_writer;
  std::string m_basic_name_of_open_file;

  daqdataformats::run_number_t m_run_number{ 0 };
  bool m_run_is_for_test_purposes{ false };
  std::atomic<size_t> m_file_index{ 0 }; // files started in the current run
  const std::string m_writer_identifier;

  const appmodel::DataStoreConf* m_config_params;
  const confmodel::Session* m_session;
  const daqdataformats::SourceIDGeoIDMap m_source_id_geo_id_map;

  const unsigned m_compression_level; // NOLINT(build/unsigned)
  const std::string m_operational_environment;
  const std::string m_offline_data_stream;
  const std::string m_operation_mode;
  const std::string m_path;
  const size_t m_max_file_size;
  const bool m_disable_unique_suffix;
  const float m_free_space_safety_factor_for_write;

  // Sizes of the open file
  std::atomic<size_t> m_recorded_size{ 0 };              // data as stored
  std::atomic<size_t> m_uncompressed_raw_data_size{ 0 }; // data before compression
  std::atomic<size_t> m_total_file_size{ 0 };            // the whole file, including format overhead
  std::atomic<size_t> m_previous_file_size{ 0 };         // for the change made by each write

  // The record being written; one record may arrive in several writes (long readout windows)
  size_t m_current_record_number{ s_unset_record_number };

  // For operational monitoring, since the last report
  std::atomic<uint64_t> m_new_bytes{ 0 };   // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_new_objects{ 0 }; // NOLINT(build/unsigned)
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_INCLUDE_DFMODULES_FILEDATASTORE_HPP_
