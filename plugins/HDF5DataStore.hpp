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

#include "TRACE/trace.h"
#include "appfwk/DAQModule.hpp"
#include "ers/Issue.h"

#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/lexical_cast.hpp"
#include "highfive/H5File.hpp"

#include <functional>
#include <memory>
#include <stdlib.h>
#include <string>
#include <sys/statvfs.h>
#include <utility>
#include <vector>

namespace dunedaq {

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidOperationMode,
                       appfwk::GeneralDAQModuleIssue,
                       "Selected opearation mode \"" << selected_operation
                                                     << "\" is NOT supported. Please update the configuration file.",
                       ((std::string)name),
                       ((std::string)selected_operation))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidHDF5Dataset,
                       appfwk::GeneralDAQModuleIssue,
                       "The HDF5 Dataset associated with name \"" << data_set << "\" is invalid. (file = " << filename
                                                                  << ")",
                       ((std::string)name),
                       ((std::string)data_set)((std::string)filename))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       HDF5DataSetError,
                       appfwk::GeneralDAQModuleIssue,
                       "DataSet exception from HighFive library for DataSet name \""
                         << data_set << "\", exception message is \"" << msgText << "\"",
                       ((std::string)name),
                       ((std::string)data_set)((std::string)msgText))

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
                         << output_path << "\". There are " << free_bytes
                         << " bytes free, and a single output file can take up to " << max_file_size_bytes << " bytes.",
                       ((std::string)name),
                       ((std::string)output_path)((size_t)free_bytes)((size_t)max_file_size_bytes))

namespace dfmodules {

/**
 * @brief HDF5DataStore creates an HDF5 instance
 * of the DataStore class
 *
 */
class HDF5DataStore : public DataStore
{

public:
  /**
   * @brief HDF5DataStore Constructor
   * @param name, path, filename, operationMode
   *
   */
  explicit HDF5DataStore(const nlohmann::json& conf)
    : DataStore(conf.value("name", "data_store"))
    , m_basic_name_of_open_file("")
    , m_open_flags_of_open_file(0)
    , m_username_substring_for_filename("_UnknownUser")
    , m_timestamp_substring_for_filename("_UnknownTime")
    , m_hashed_timeuser_substring_for_filename("_abcdef")
  {
    TLOG(TLVL_DEBUG) << get_name() << ": Configuration: " << conf;

    m_config_params = conf.get<hdf5datastore::ConfParams>();
    m_operation_mode = m_config_params.mode;
    m_path = m_config_params.directory_path;
    m_max_file_size = m_config_params.max_file_size_bytes;
    m_disable_unique_suffix = m_config_params.disable_unique_filename_suffix;

    m_file_count = 0;
    m_recorded_size = 0;

    if (m_operation_mode != "one-event-per-file" && m_operation_mode != "one-fragment-per-file" &&
        m_operation_mode != "all-per-file") {

      throw InvalidOperationMode(ERS_HERE, get_name(), m_operation_mode);
    }

    char* unp = getenv("USER");
    std::string tmp_string(unp);
    m_username_substring_for_filename = "_" + tmp_string;
    TLOG(TLVL_DEBUG) << get_name() << ": m_username_substring_for_filename: " << m_username_substring_for_filename;
  }

  virtual KeyedDataBlock read(const StorageKey& key)
  {
    TLOG(TLVL_DEBUG) << get_name() << ": going to read data block from triggerNumber/detectorType/apaNumber/linkNumber "
                     << HDF5KeyTranslator::get_path_string(key, m_config_params.file_layout_parameters) << " from file "
                     << HDF5KeyTranslator::get_file_name(key, m_config_params, m_file_count);

    // opening the file from Storage Key + configuration parameters
    std::string full_filename = HDF5KeyTranslator::get_file_name(key, m_config_params, m_file_count);

    // m_file_ptr will be the handle to the Opened-File after a call to open_file_if_needed()
    open_file_if_needed(full_filename, HighFive::File::ReadOnly);

    std::vector<std::string> group_and_dataset_path_elements =
      HDF5KeyTranslator::get_path_elements(key, m_config_params.file_layout_parameters);

    // const std::string dataset_name = std::to_string(key.get_link_number());
    const std::string dataset_name = group_and_dataset_path_elements.back();

    KeyedDataBlock data_block(key);

    HighFive::Group hdf5_group = HDF5FileUtils::get_subgroup(m_file_ptr, group_and_dataset_path_elements, false);

    try { // to determine if the dataset exists in the group and copy it to membuffer
      HighFive::DataSet data_set = hdf5_group.getDataSet(dataset_name);
      data_block.m_data_size = data_set.getStorageSize();
      HighFive::DataSpace thedataSpace = data_set.getSpace();
      char* membuffer = new char[data_block.m_data_size];
      data_set.read(membuffer);
      std::unique_ptr<char> mem_ptr(membuffer);
      data_block.m_owned_data_start = std::move(mem_ptr);
    } catch (HighFive::DataSetException const&) {
      ERS_LOG("HDF5DataSet " << dataset_name << " not found.");
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
    // opening the file from Storage Key + configuration parameters
    std::string full_filename = HDF5KeyTranslator::get_file_name(data_block.m_data_key, m_config_params, m_file_count);

    // m_file_ptr will be the handle to the Opened-File after a call to open_file_if_needed()
    open_file_if_needed(full_filename, HighFive::File::OpenOrCreate);

    TLOG(TLVL_DEBUG) << get_name() << ": Writing data with run number " << data_block.m_data_key.get_run_number()
                     << " and trigger number " << data_block.m_data_key.get_trigger_number() << " and detector type "
                     << data_block.m_data_key.get_detector_type() << " and apa/link number "
                     << data_block.m_data_key.get_apa_number() << " / " << data_block.m_data_key.get_link_number();

    std::vector<std::string> group_and_dataset_path_elements =
      HDF5KeyTranslator::get_path_elements(data_block.m_data_key, m_config_params.file_layout_parameters);

    const std::string dataset_name = group_and_dataset_path_elements.back();

    HighFive::Group sub_group = HDF5FileUtils::get_subgroup(m_file_ptr, group_and_dataset_path_elements, true);

    // Create dataset
    HighFive::DataSpace data_space = HighFive::DataSpace({ data_block.m_data_size, 1 });
    HighFive::DataSetCreateProps data_set_create_props;
    HighFive::DataSetAccessProps data_set_access_props;

    try {
      auto data_set =
        sub_group.createDataSet<char>(dataset_name, data_space, data_set_create_props, data_set_access_props);
      if (data_set.isValid()) {
        data_set.write_raw(static_cast<const char*>(data_block.get_data_start()));
      } else {
        throw InvalidHDF5Dataset(ERS_HERE, get_name(), dataset_name, m_file_ptr->getName());
      }
    } catch (HighFive::DataSetException const& excpt) {
      ers::error(HDF5DataSetError(ERS_HERE, get_name(), dataset_name, excpt.what()));
    } catch (HighFive::Exception const& excpt) {
      ERS_LOG("Exception: " << excpt.what());
    }

    m_file_ptr->flush();
    m_recorded_size += data_block.m_data_size;

    // 13-Jan-2021, KAB: disable the file size checking, for now.
    // AAA: be careful on the units, maybe force it somehow?
    // if (m_recorded_size > m_max_file_size) {
    //  m_file_count++;
    //}
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

    for (auto& filename : fileList) {
      std::unique_ptr<HighFive::File> local_file_ptr(new HighFive::File(filename, HighFive::File::ReadOnly));
      TLOG(TLVL_DEBUG) << get_name() << ": Opened HDF5 file " << filename;

      std::vector<std::string> pathList = HDF5FileUtils::get_all_dataset_paths(*local_file_ptr);
      TLOG(TLVL_DEBUG) << get_name() << ": Path list has element count: " << pathList.size();

      for (auto& path : pathList) {
        StorageKey thisKey(0, 0, "", 0, 0);
        thisKey = HDF5KeyTranslator::get_key_from_string(path);
        keyList.push_back(thisKey);
      }

      local_file_ptr.reset(); // explicit destruction
    }

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
    TLOG(TLVL_DEBUG) << get_name() << ": Preparing to get the statvfs results for path: \"" << m_path << "\"";

    int retval = statvfs(m_path.c_str(), &vfs_results);
    TLOG(TLVL_DEBUG) << get_name() << ": statvfs return code is " << retval;
    if (retval != 0) {
      throw InvalidOutputPath(ERS_HERE, get_name(), m_path);
    }

    size_t free_space = vfs_results.f_bsize * vfs_results.f_bavail;
    TLOG(TLVL_DEBUG) << get_name() << ": Free space on disk with path \"" << m_path << "\" is " << free_space
                     << " bytes.";
    if (free_space < m_max_file_size) {
      throw InsufficientDiskSpace(ERS_HERE, get_name(), m_path, free_space, m_max_file_size);
    }

    // create the timestamp substring that is part of unique-ifying the filename, for later use
    time_t now = time(0);
    m_timestamp_substring_for_filename = "_" + boost::posix_time::to_iso_string(boost::posix_time::from_time_t(now));
    TLOG(TLVL_DEBUG) << get_name() << ": m_timestamp_substring_for_filename: " << m_timestamp_substring_for_filename;

    // the following code demonstrates how we could create a unique hash for use in the filename
    std::string work_string = m_username_substring_for_filename + m_timestamp_substring_for_filename;
    std::hash<std::string> string_hasher;
    std::ostringstream oss_hex;
    oss_hex << std::hex << string_hasher(work_string);
    m_hashed_timeuser_substring_for_filename = "_" + oss_hex.str();
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
    if (m_file_ptr.get() != nullptr) {
      m_file_ptr->flush();
      m_file_ptr.reset();
    }
  }

private:
  HDF5DataStore(const HDF5DataStore&) = delete;
  HDF5DataStore& operator=(const HDF5DataStore&) = delete;
  HDF5DataStore(HDF5DataStore&&) = delete;
  HDF5DataStore& operator=(HDF5DataStore&&) = delete;

  std::unique_ptr<HighFive::File> m_file_ptr;
  std::string m_basic_name_of_open_file;
  unsigned m_open_flags_of_open_file;

  std::string m_username_substring_for_filename;
  std::string m_timestamp_substring_for_filename;
  std::string m_hashed_timeuser_substring_for_filename;

  // Total number of generated files
  size_t m_file_count;

  // Total size of data being written
  size_t m_recorded_size;

  // Configuration
  hdf5datastore::ConfParams m_config_params;
  std::string m_operation_mode;
  std::string m_path;
  size_t m_max_file_size;
  bool m_disable_unique_suffix;

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

  void open_file_if_needed(const std::string& file_name, unsigned open_flags = HighFive::File::ReadOnly)
  {

    if (m_basic_name_of_open_file.compare(file_name) || m_open_flags_of_open_file != open_flags) {

      // 04-Feb-2021, KAB: adding unique substrings to the filename
      std::string unique_filename = file_name;
      if (!m_disable_unique_suffix) {
        size_t ufn_len;
        // username substring
        ufn_len = unique_filename.length();
        if (ufn_len > 6) {
          unique_filename.insert(ufn_len - 5, m_username_substring_for_filename);
        }
        // timestamp substring
        ufn_len = unique_filename.length();
        if (ufn_len > 6) {
          unique_filename.insert(ufn_len - 5, m_timestamp_substring_for_filename);
        }
#if 0
// KAB - skip this in favor of the individual username and timestamp fields, for now
        // timestamp and username hash
        ufn_len = unique_filename.length();
        if (ufn_len > 6) {
          unique_filename.insert(ufn_len - 5, m_hashed_timeuser_substring_for_filename);
        }
#endif
      }

      // opening file for the first time OR something changed in the name or the way of opening the file
      TLOG(TLVL_DEBUG) << get_name() << ": going to open file " << unique_filename << " with open_flags "
                       << std::to_string(open_flags);
      m_basic_name_of_open_file = file_name;
      m_open_flags_of_open_file = open_flags;
      m_file_ptr.reset(new HighFive::File(unique_filename, open_flags));
      TLOG(TLVL_DEBUG) << get_name() << "Created HDF5 file.";

    } else {

      TLOG(TLVL_DEBUG) << get_name() << ": Pointer file to  " << m_basic_name_of_open_file
                       << " was already opened with open_flags " << std::to_string(m_open_flags_of_open_file);
    }
  }
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_HDF5DATASTORE_HPP_

// Local Variables:
// c-basic-offset: 2
// End:
