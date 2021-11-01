/**
 * @file HDF5DataStore.hpp
 *
 * An implementation of the DataStore interface that uses the
 * highFive library to create objects in HDF5 Groups and datasets
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_HDF5DATASTORE_HPP_
#define DFMODULES_PLUGINS_HDF5DATASTORE_HPP_

#include "HDF5FileUtils.hpp"
#include "HDF5KeyTranslator.hpp"
#include "dfmodules/DataStore.hpp"
#include "dfmodules/hdf5datastore/Nljs.hpp"
#include "dfmodules/hdf5datastore/Structs.hpp"

#include "appfwk/DAQModule.hpp"
#include "logging/Logging.hpp"

#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/lexical_cast.hpp"
#include "highfive/H5File.hpp"

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
 * @brief A ERS Issue to report an HDF5 exception
 */
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
                       InvalidHDF5Dataset,
                       appfwk::GeneralDAQModuleIssue,
                       "The HDF5 Dataset associated with name \"" << data_set << "\" is invalid. (file = " << filename
                                                                  << ")",
                       ((std::string)name),
                       ((std::string)data_set)((std::string)filename))

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
                       EmptyDataBlockList,
                       appfwk::GeneralDAQModuleIssue,
                       "There was a request to write out a list of data blocks, but the list was empty. "
                         << "Ignoring this request",
                       ((std::string)name),
                       ERS_EMPTY)

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       NullFilePointer,
                       appfwk::GeneralDAQModuleIssue,
                       "There was an attempt to create a FileHandle from a null file pointer. "
                         << "This is not allowed because later uses of the file pointer assume that it is not null. ",
                       ((std::string)name),
                       ERS_EMPTY)

// Re-enable coverage checking LCOV_EXCL_STOP
namespace dfmodules {

/**
 * @brief HDF5FileHandle is a small helper class that takes care
 * of common actions on HighFive::File instances.
 */
class HDF5FileHandle
{
public:
  inline static const std::string s_inprogress_filename_suffix = ".writing";

  explicit HDF5FileHandle(std::unique_ptr<HighFive::File> file_ptr)
  {
    if (file_ptr.get() == nullptr) {
      throw NullFilePointer(ERS_HERE, "HDF5FileHandle");
    }
    m_file_ptr = std::move(file_ptr);
  }

  ~HDF5FileHandle()
  {
    if (m_file_ptr.get() != nullptr) {
      m_file_ptr->flush();

      std::string open_filename = m_file_ptr->getName();
      std::string final_filename = open_filename;
      int pos = open_filename.find(s_inprogress_filename_suffix);
      final_filename.erase(pos);
      std::filesystem::rename(open_filename, final_filename);

      m_file_ptr.reset(); // explicit destruction; not really needed...
    }
  }

  HighFive::File* get_file_ptr() const { return m_file_ptr.get(); }

  std::unique_ptr<HighFive::File> m_file_ptr;
};

/**
 * @brief HDF5DataStore creates an HDF5 instance
 * of the DataStore class
 */
class HDF5DataStore : public DataStore
{

public:
  enum
  {
    TLVL_BASIC = 2,
    TLVL_FILE_SIZE = 5
  };

  /**
   * @brief HDF5DataStore Constructor
   * @param name, path, filename, operationMode
   *
   */
  explicit HDF5DataStore(const nlohmann::json& conf)
    : DataStore(conf.value("name", "data_store"))
    , m_basic_name_of_open_file("")
    , m_open_flags_of_open_file(0)
    , m_timestamp_substring_for_filename("_UnknownTime")
  {
    TLOG_DEBUG(TLVL_BASIC) << get_name() << ": Configuration: " << conf;

    m_config_params = conf.get<hdf5datastore::ConfParams>();
    m_key_translator_ptr.reset(new HDF5KeyTranslator(m_config_params));
    m_operation_mode = m_config_params.mode;
    m_path = m_config_params.directory_path;
    m_max_file_size = m_config_params.max_file_size_bytes;
    m_disable_unique_suffix = m_config_params.disable_unique_filename_suffix;
    m_free_space_safety_factor_for_write = m_config_params.free_space_safety_factor_for_write;
    if (m_free_space_safety_factor_for_write < 1.1) {
      m_free_space_safety_factor_for_write = 1.1;
    }

    m_file_index = 0;
    m_recorded_size = 0;

    if (m_operation_mode != "one-event-per-file" && m_operation_mode != "one-fragment-per-file" &&
        m_operation_mode != "all-per-file") {

      throw InvalidOperationMode(ERS_HERE, get_name(), m_operation_mode);
    }
  }

  virtual KeyedDataBlock read(const StorageKey& key)
  {
    TLOG_DEBUG(TLVL_BASIC) << get_name()
                           << ": going to read data block from triggerNumber/groupType/regionNumber/elementNumber "
                           << m_key_translator_ptr->get_path_string(key) << " from file "
                           << m_key_translator_ptr->get_file_name(key, m_file_index);

    // opening the file from Storage Key + configuration parameters
    std::string full_filename = m_key_translator_ptr->get_file_name(key, m_file_index);

    try {
      open_file_if_needed(full_filename, HighFive::File::ReadOnly);
    } catch (std::exception const& excpt) {
      throw FileOperationProblem(ERS_HERE, get_name(), full_filename, excpt);
    } catch (...) { // NOLINT(runtime/exceptions)
      // NOLINT here because we *ARE* re-throwing the exception!
      throw FileOperationProblem(ERS_HERE, get_name(), full_filename);
    }

    std::vector<std::string> group_and_dataset_path_elements = m_key_translator_ptr->get_path_elements(key);

    // const std::string dataset_name = std::to_string(key.get_element_number());
    const std::string dataset_name = group_and_dataset_path_elements.back();

    KeyedDataBlock data_block(key);

    HighFive::Group hdf5_group =
      HDF5FileUtils::get_subgroup(m_file_handle->get_file_ptr(), group_and_dataset_path_elements, false);

    try { // to determine if the dataset exists in the group and copy it to membuffer
      HighFive::DataSet data_set = hdf5_group.getDataSet(dataset_name);
      data_block.m_data_size = data_set.getStorageSize();
      HighFive::DataSpace thedataSpace = data_set.getSpace();
      char* membuffer = new char[data_block.m_data_size];
      data_set.read(membuffer);
      std::unique_ptr<char> mem_ptr(membuffer);
      data_block.m_owned_data_start = std::move(mem_ptr);
    } catch (HighFive::DataSetException const&) {
      TLOG() << "HDF5DataSet " << dataset_name << " not found.";
    }

    return data_block;
  }

  /**
   * @brief HDF5DataStore write()
   * Method used to write constant data
   * into HDF5 format. Operational mode
   * defined in the configuration file.
   *
   */
  virtual void write(const KeyedDataBlock& data_block)
  {
    // check if there is sufficient space for this data block
    size_t current_free_space = get_free_space(m_path);
    if (current_free_space < (m_free_space_safety_factor_for_write * data_block.m_data_size)) {
      InsufficientDiskSpace issue(ERS_HERE,
                                  get_name(),
                                  m_path,
                                  current_free_space,
                                  (data_block.m_data_size),
                                  "a multiple of the data block size");
      std::string msg = "writing a data block to file " + m_file_handle->get_file_ptr()->getName();
      throw RetryableDataStoreProblem(ERS_HERE, get_name(), msg, issue);
    }

    // check if a new file should be opened for this data block
    increment_file_index_if_needed(data_block.m_data_size);

    // determine the filename from Storage Key + configuration parameters
    std::string full_filename = m_key_translator_ptr->get_file_name(data_block.m_data_key, m_file_index);

    try {
      open_file_if_needed(full_filename, HighFive::File::OpenOrCreate);
    } catch (std::exception const& excpt) {
      throw FileOperationProblem(ERS_HERE, get_name(), full_filename, excpt);
    } catch (...) { // NOLINT(runtime/exceptions)
      // NOLINT here because we *ARE* re-throwing the exception!
      throw FileOperationProblem(ERS_HERE, get_name(), full_filename);
    }

    // write the data block
    do_write(data_block);
  }

  /**
   * @brief Writes the specified set of data blocks into the DataStore.
   * @param data_block_list List of data blocks to write.
   */
  virtual void write(const std::vector<KeyedDataBlock>& data_block_list)
  {
    // check that the list has at least one entry
    if (data_block_list.size() < 1) {
      throw EmptyDataBlockList(ERS_HERE, get_name());
    }

    // check if there is sufficient space for these data blocks
    size_t sum_of_sizes = 0;
    for (auto& data_block : data_block_list) {
      sum_of_sizes += data_block.m_data_size;
    }
    size_t current_free_space = get_free_space(m_path);
    if (current_free_space < (m_free_space_safety_factor_for_write * sum_of_sizes)) {
      InsufficientDiskSpace issue(ERS_HERE,
                                  get_name(),
                                  m_path,
                                  current_free_space,
                                  (m_free_space_safety_factor_for_write * sum_of_sizes),
                                  "a multiple of the data block sizes");
      std::string msg = "writing a list of data blocks to file " + m_file_handle->get_file_ptr()->getName();
      throw RetryableDataStoreProblem(ERS_HERE, get_name(), msg, issue);
    }

    // check if a new file should be opened for this set of data blocks
    TLOG_DEBUG(TLVL_FILE_SIZE) << get_name() << ": Checking file size, recorded=" << m_recorded_size
                               << ", additional=" << sum_of_sizes << ", max=" << m_max_file_size;
    increment_file_index_if_needed(sum_of_sizes);

    // determine the filename from Storage Key + configuration parameters
    // (This assumes that all of the blocks have a data_key that puts them in the same file...)
    std::string full_filename = m_key_translator_ptr->get_file_name(data_block_list[0].m_data_key, m_file_index);

    try {
      open_file_if_needed(full_filename, HighFive::File::OpenOrCreate);
    } catch (std::exception const& excpt) {
      throw FileOperationProblem(ERS_HERE, get_name(), full_filename, excpt);
    } catch (...) { // NOLINT(runtime/exceptions)
      // NOLINT here because we *ARE* re-throwing the exception!
      throw FileOperationProblem(ERS_HERE, get_name(), full_filename);
    }

    // write each data block
    for (auto& data_block : data_block_list) {
      do_write(data_block);
    }
  }

  /**
   * @brief HDF5DataStore get_all_existing_keys
   * Method used to retrieve a vector with all
   * the StorageKeys
   *
   */
  virtual std::vector<StorageKey> get_all_existing_keys() const
  {
    std::vector<StorageKey> keyList;
    std::vector<std::string> fileList; // = get_all_files();

#if 0
    for (auto& filename : fileList) {
      std::unique_ptr<HighFive::File> local_file_ptr(new HighFive::File(filename, HighFive::File::ReadOnly));
      TLOG_DEBUG(TLVL_BASIC) << get_name() << ": Opened HDF5 file " << filename;

      std::vector<std::string> pathList = HDF5FileUtils::get_all_dataset_paths(*local_file_ptr);
      TLOG_DEBUG(TLVL_BASIC) << get_name() << ": Path list has element count: " << pathList.size();

      for (auto& path : pathList) {
        StorageKey thisKey(0, 0, StorageKey::DataRecordGroupType::kInvalid, 0, 0);
        thisKey = HDF5KeyTranslator::get_key_from_string(path);
        keyList.push_back(thisKey);
      }

      local_file_ptr.reset(); // explicit destruction
    }
#endif

    return keyList;
  }

  /**
   * @brief Informs the HDF5DataStore that writes or reads of data blocks
   * associated with the specified run number will soon be requested.
   * This allows the DataStore to test that the output file path is valid
   * and any other checks that are useful in advance of the first data
   * blocks being written or read.
   *
   * This method may throw an exception if it finds a problem.
   */
  void prepare_for_run(dataformats::run_number_t /*run_number*/)
  {
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

    // create the timestamp substring that is part of unique-ifying the filename, for later use
    time_t now = time(0);
    m_timestamp_substring_for_filename = "_" + boost::posix_time::to_iso_string(boost::posix_time::from_time_t(now));
    TLOG_DEBUG(TLVL_BASIC) << get_name()
                           << ": m_timestamp_substring_for_filename: " << m_timestamp_substring_for_filename;

    m_file_index = 0;
    m_recorded_size = 0;
  }

  /**
   * @brief Informs the HD5DataStore that writes or reads of data blocks
   * associated with the specified run number have finished, for now.
   * This allows the DataStore to close open files and do any other
   * cleanup or shutdown operations that are useful once the writes or
   * reads for a given run number have finished.
   */
  void finish_with_run(dataformats::run_number_t /*run_number*/)
  {
    if (m_file_handle.get() != nullptr) {
      std::string open_filename = m_file_handle->get_file_ptr()->getName();
      try {
        m_file_handle.reset();
      } catch (std::exception const& excpt) {
        throw FileOperationProblem(ERS_HERE, get_name(), open_filename, excpt);
      } catch (...) { // NOLINT(runtime/exceptions)
        // NOLINT here because we *ARE* re-throwing the exception!
        throw FileOperationProblem(ERS_HERE, get_name(), open_filename);
      }
    }
  }

private:
  HDF5DataStore(const HDF5DataStore&) = delete;
  HDF5DataStore& operator=(const HDF5DataStore&) = delete;
  HDF5DataStore(HDF5DataStore&&) = delete;
  HDF5DataStore& operator=(HDF5DataStore&&) = delete;

  std::unique_ptr<HDF5FileHandle> m_file_handle;
  std::string m_basic_name_of_open_file;
  unsigned m_open_flags_of_open_file;

  std::string m_timestamp_substring_for_filename;

  // Total number of generated files
  size_t m_file_index;

  // Total size of data being written
  size_t m_recorded_size;

  // Configuration
  hdf5datastore::ConfParams m_config_params;
  std::string m_operation_mode;
  std::string m_path;
  size_t m_max_file_size;
  bool m_disable_unique_suffix;
  float m_free_space_safety_factor_for_write;

  std::unique_ptr<HDF5KeyTranslator> m_key_translator_ptr;

#if 0
  std::vector<std::string> get_all_files() const
  {
    std::string work_string = m_filename_prefix;
    if (m_operation_mode == "one-event-per-file") {
      work_string += "_event_\\d+.hdf5";
    } else if (m_operation_mode == "one-fragment-per-file") {
      work_string += "_event_\\d+_geoID_\\d+.hdf5";
    } else {
      work_string += "_all_events.hdf5";
    }

    return HDF5FileUtils::get_files_matching_pattern(m_path, work_string);
  }
#endif

  void do_write(const KeyedDataBlock& data_block)
  {
    TLOG_DEBUG(TLVL_BASIC) << get_name() << ": Writing data with run number " << data_block.m_data_key.get_run_number()
                           << " and trigger number " << data_block.m_data_key.get_trigger_number() << " and group type "
                           << data_block.m_data_key.get_group_type() << " and region/element number "
                           << data_block.m_data_key.get_region_number() << " / "
                           << data_block.m_data_key.get_element_number();
    HighFive::File* hdf_file_ptr = m_file_handle->get_file_ptr();

    std::vector<std::string> group_and_dataset_path_elements =
      m_key_translator_ptr->get_path_elements(data_block.m_data_key);

    const std::string dataset_name = group_and_dataset_path_elements.back();

    HighFive::Group sub_group = HDF5FileUtils::get_subgroup(hdf_file_ptr, group_and_dataset_path_elements, true);

    // Create dataset
    HighFive::DataSpace data_space = HighFive::DataSpace({ data_block.m_data_size, 1 });
    HighFive::DataSetCreateProps data_set_create_props;
    HighFive::DataSetAccessProps data_set_access_props;

    try {
      auto data_set =
        sub_group.createDataSet<char>(dataset_name, data_space, data_set_create_props, data_set_access_props);
      if (data_set.isValid()) {
        data_set.write_raw(static_cast<const char*>(data_block.get_data_start()));
        hdf_file_ptr->flush();
        m_recorded_size += data_block.m_data_size;
      } else {
        throw InvalidHDF5Dataset(ERS_HERE, get_name(), dataset_name, hdf_file_ptr->getName());
      }
    } catch (std::exception const& excpt) {
      std::string description = "DataSet " + dataset_name;
      std::string msg = "writing DataSet " + dataset_name + " to file " + hdf_file_ptr->getName();
      throw GeneralDataStoreProblem(ERS_HERE, get_name(), msg, excpt);
    } catch (...) { // NOLINT(runtime/exceptions)
      // NOLINT here because we *ARE* re-throwing the exception!
      std::string msg = "writing DataSet " + dataset_name + " to file " + hdf_file_ptr->getName();
      throw GeneralDataStoreProblem(ERS_HERE, get_name(), msg);
    }
  }

  void increment_file_index_if_needed(size_t size_of_next_write)
  {
    if ((m_recorded_size + size_of_next_write) > m_max_file_size && m_recorded_size > 0) {
      ++m_file_index;
      m_recorded_size = 0;
    }
  }

  void open_file_if_needed(const std::string& file_name, unsigned open_flags = HighFive::File::ReadOnly)
  {

    if (m_file_handle.get() == nullptr || m_basic_name_of_open_file.compare(file_name) ||
        m_open_flags_of_open_file != open_flags) {

      // 04-Feb-2021, KAB: adding unique substrings to the filename
      std::string unique_filename = file_name;
      if (!m_disable_unique_suffix) {
        size_t ufn_len;
        // timestamp substring
        ufn_len = unique_filename.length();
        if (ufn_len > 6) {
          unique_filename.insert(ufn_len - 5, m_timestamp_substring_for_filename);
        }
      }

      // close an existing open file
      if (m_file_handle.get() != nullptr) {
        std::string open_filename = m_file_handle->get_file_ptr()->getName();
        try {
          m_file_handle.reset();
        } catch (std::exception const& excpt) {
          throw FileOperationProblem(ERS_HERE, get_name(), open_filename, excpt);
        } catch (...) { // NOLINT(runtime/exceptions)
          // NOLINT here because we *ARE* re-throwing the exception!
          throw FileOperationProblem(ERS_HERE, get_name(), open_filename);
        }
      }

      // opening file for the first time OR something changed in the name or the way of opening the file
      TLOG_DEBUG(TLVL_BASIC) << get_name() << ": going to open file " << unique_filename << " with open_flags "
                             << std::to_string(open_flags);
      m_basic_name_of_open_file = file_name;
      m_open_flags_of_open_file = open_flags;
      unique_filename += HDF5FileHandle::s_inprogress_filename_suffix;
      try {
        std::unique_ptr<HighFive::File> tmp_file_ptr(new HighFive::File(unique_filename, open_flags));
        m_file_handle.reset(new HDF5FileHandle(std::move(tmp_file_ptr)));
      } catch (std::exception const& excpt) {
        throw FileOperationProblem(ERS_HERE, get_name(), unique_filename, excpt);
      } catch (...) { // NOLINT(runtime/exceptions)
        // NOLINT here because we *ARE* re-throwing the exception!
        throw FileOperationProblem(ERS_HERE, get_name(), unique_filename);
      }

      if (open_flags == HighFive::File::ReadOnly) {
        TLOG_DEBUG(TLVL_BASIC) << get_name() << "Opened HDF5 file read-only.";
      } else {
        TLOG_DEBUG(TLVL_BASIC) << get_name() << "Created HDF5 file (" << unique_filename << ").";

        if (!m_file_handle->get_file_ptr()->hasAttribute("data_format_version")) {
          int version = m_key_translator_ptr->get_current_version();
          m_file_handle->get_file_ptr()->createAttribute("data_format_version", version);
        }
        if (!m_file_handle->get_file_ptr()->hasAttribute("operational_environment")) {
          std::string op_env_type = m_config_params.operational_environment;
          m_file_handle->get_file_ptr()->createAttribute("operational_environment", op_env_type);
        }
      }
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

#endif // DFMODULES_PLUGINS_HDF5DATASTORE_HPP_

// Local Variables:
// c-basic-offset: 2
// End:
