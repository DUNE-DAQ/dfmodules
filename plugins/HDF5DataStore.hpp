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

#include <TRACE/trace.h>
#include <appfwk/DAQModule.hpp>
#include <ers/Issue.h>

#include <boost/lexical_cast.hpp>
#include <highfive/H5File.hpp>

#include <memory>
#include <string>
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
                       "DataSet exception from HighFive library for DataSet name \"" << data_set
                       << "\", exception message is \"" << msgText << "\"",
                       ((std::string)name),
                       ((std::string)data_set)((std::string)msgText))

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
    , m_full_name_of_open_file("")
    , m_open_flags_of_open_file(0)
  {
    TLOG(TLVL_DEBUG) << get_name() << ": Configuration: " << conf;

    m_config_params = conf.get<hdf5datastore::ConfParams>();
    m_filename_prefix = m_config_params.filename_parameters.overall_prefix;
    m_path = m_config_params.directory_path;
    m_operation_mode = m_config_params.mode;
    m_max_file_size = m_config_params.max_file_size_bytes;
    // m_filename_prefix = conf["filename_prefix"].get<std::string>() ;
    // m_path = conf["directory_path"].get<std::string>() ;
    // m_operation_mode = conf["mode"].get<std::string>() ;
    // Get maximum file size in bytes
    // AAA: todo get from configuration params, force that max file size to be in bytes?
    // m_max_file_size = conf["MaxFileSize"].get()*1024ULL*1024ULL;
    // m_max_file_size = 1*1024ULL*1024ULL;
    m_file_count = 0;
    m_recorded_size = 0;

    if (m_operation_mode != "one-event-per-file" && m_operation_mode != "one-fragment-per-file" &&
        m_operation_mode != "all-per-file") {

      throw InvalidOperationMode(ERS_HERE, get_name(), m_operation_mode);
    }
  }

  virtual void setup(const size_t eventId) { ERS_INFO("Setup ... " << eventId); }

  virtual KeyedDataBlock read(const StorageKey& key)
  {
    TLOG(TLVL_DEBUG) << get_name() << ": going to read data block from triggerNumber/detectorType/apaNumber/linkNumber "
                     << HDF5KeyTranslator::get_path_string(key, m_config_params.file_layout_parameters) << " from file "
                     << HDF5KeyTranslator::get_file_name(key, m_config_params, m_file_count);

    // opening the file from Storage Key + m_path + m_filename_prefix + m_operation_mode
    std::string full_filename = HDF5KeyTranslator::get_file_name(key, m_config_params, m_file_count);

    // file_ptr will be the handle to the Opened-File after a call to open_file_if_needed()
    open_file_if_needed(full_filename, HighFive::File::ReadOnly);

    std::vector<std::string> group_and_dataset_path_elements =
      HDF5KeyTranslator::get_path_elements(key, m_config_params.file_layout_parameters);

    // const std::string dataset_name = std::to_string(key.get_link_number());
    const std::string dataset_name = group_and_dataset_path_elements.back();

    KeyedDataBlock data_block(key);

    HighFive::Group hdf5_group = HDF5FileUtils::get_subgroup(file_ptr, group_and_dataset_path_elements, false);

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
    // opening the file from Storage Key + m_path + m_filename_prefix + m_operation_mode
    std::string full_filename = HDF5KeyTranslator::get_file_name(data_block.m_data_key, m_config_params, m_file_count);

    // file_ptr will be the handle to the Opened-File after a call to open_file_if_needed()
    open_file_if_needed(full_filename, HighFive::File::OpenOrCreate);

    TLOG(TLVL_DEBUG) << get_name() << ": Writing data with run number " << data_block.m_data_key.get_run_number()
                     << " and trigger number " << data_block.m_data_key.get_trigger_number() << " and detector type "
                     << data_block.m_data_key.get_detector_type() << " and apa/link number "
                     << data_block.m_data_key.get_apa_number() << " / " << data_block.m_data_key.get_link_number();

    std::vector<std::string> group_and_dataset_path_elements =
      HDF5KeyTranslator::get_path_elements(data_block.m_data_key, m_config_params.file_layout_parameters);

    const std::string dataset_name = group_and_dataset_path_elements.back();

    HighFive::Group sub_group = HDF5FileUtils::get_subgroup(file_ptr, group_and_dataset_path_elements, true);

    // Create dataset
    HighFive::DataSpace data_space = HighFive::DataSpace({ data_block.m_data_size, 1 });
    HighFive::DataSetCreateProps data_set_create_props;
    HighFive::DataSetAccessProps data_set_access_props;

    try {
      auto data_set = sub_group.createDataSet<char>(dataset_name, data_space, data_set_create_props, data_set_access_props);
      if (data_set.isValid()) {
        data_set.write_raw(static_cast<const char*>(data_block.get_data_start()));
      } else {
        throw InvalidHDF5Dataset(ERS_HERE, get_name(), dataset_name, file_ptr->getName());
      } 
    } catch (HighFive::DataSetException const& excpt) {
      ers::error(HDF5DataSetError(ERS_HERE, get_name(), dataset_name, excpt.what()));
    } catch (HighFive::Exception const& excpt) {
      ERS_LOG("Exception: " << excpt.what());
    }

    file_ptr->flush();
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
    std::vector<std::string> fileList = get_all_files();

    for (auto& filename : fileList) {
      std::unique_ptr<HighFive::File> localFilePtr(new HighFive::File(filename, HighFive::File::ReadOnly));
      TLOG(TLVL_DEBUG) << get_name() << ": Opened HDF5 file " << filename;

      std::vector<std::string> pathList = HDF5FileUtils::get_all_dataset_paths(*localFilePtr);
      TLOG(TLVL_DEBUG) << get_name() << ": Path list has element count: " << pathList.size();

      for (auto& path : pathList) {
        StorageKey thisKey(0, 0, "", 0, 0);
        thisKey = HDF5KeyTranslator::get_key_from_string(path);
        keyList.push_back(thisKey);
      }

      localFilePtr.reset(); // explicit destruction
    }

    return keyList;
  }

private:
  HDF5DataStore(const HDF5DataStore&) = delete;
  HDF5DataStore& operator=(const HDF5DataStore&) = delete;
  HDF5DataStore(HDF5DataStore&&) = delete;
  HDF5DataStore& operator=(HDF5DataStore&&) = delete;

  std::unique_ptr<HighFive::File> file_ptr;

  std::string m_path;
  std::string m_filename_prefix;
  std::string m_operation_mode;
  std::string m_full_name_of_open_file;
  size_t m_max_file_size;

  // Total number of generated files
  size_t m_file_count;

  // Total size of data being written
  size_t m_recorded_size;

  unsigned m_open_flags_of_open_file;

  // Configuration
  hdf5datastore::ConfParams m_config_params;

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

  void open_file_if_needed(const std::string& fileName, unsigned open_flags = HighFive::File::ReadOnly)
  {

    if (m_full_name_of_open_file.compare(fileName) || m_open_flags_of_open_file != open_flags) {

      // opening file for the first time OR something changed in the name or the way of opening the file
      TLOG(TLVL_DEBUG) << get_name() << ": going to open file " << fileName << " with open_flags "
                       << std::to_string(open_flags);
      m_full_name_of_open_file = fileName;
      m_open_flags_of_open_file = open_flags;
      file_ptr.reset(new HighFive::File(fileName, open_flags));
      TLOG(TLVL_DEBUG) << get_name() << "Created HDF5 file.";

    } else {

      TLOG(TLVL_DEBUG) << get_name() << ": Pointer file to  " << m_full_name_of_open_file
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
