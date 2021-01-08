#ifndef DFMODULES_SRC_HDF5DATASTORE_HPP_
#define DFMODULES_SRC_HDF5DATASTORE_HPP_

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

#include "dfmodules/DataStore.hpp"
#include "dfmodules/hdf5datastore/Nljs.hpp"
#include "dfmodules/hdf5datastore/Structs.hpp"
#include "HDF5FileUtils.hpp"
#include "HDF5KeyTranslator.hpp"

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
                       "The HDF5 Dataset associated with name \"" << dataSet << "\" is invalid. (file = " << filename
                                                                  << ")",
                       ((std::string)name),
                       ((std::string)dataSet)((std::string)filename))

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
   * @param name, path, fileName, operationMode
   *
   */
  explicit HDF5DataStore( const nlohmann::json & conf )
    : DataStore( conf.value("name", "data_store") )
    , fullNameOfOpenFile_("")
    , openFlagsOfOpenFile_(0)
  {
    TLOG(TLVL_DEBUG) << get_name() << ": Configuration: " << conf ; 
    
    config_params_ = conf.get<hdf5datastore::ConfParams>();
    fileName_ = config_params_.filename_parameters.overall_prefix;
    path_ = config_params_.directory_path;
    operation_mode_ = config_params_.mode;
    max_file_size_ = config_params_.max_file_size_bytes;
    //fileName_ = conf["filename_prefix"].get<std::string>() ; 
    //path_ = conf["directory_path"].get<std::string>() ;
    //operation_mode_ = conf["mode"].get<std::string>() ; 
    // Get maximum file size in bytes
    // AAA: todo get from configuration params, force that max file size to be in bytes?
    //max_file_size_ = conf["MaxFileSize"].get()*1024ULL*1024ULL;
    //max_file_size_ = 1*1024ULL*1024ULL;
    file_count_ = 0;
    recorded_size_ = 0;


    if (operation_mode_ != "one-event-per-file" && operation_mode_ != "one-fragment-per-file" &&
        operation_mode_ != "all-per-file") {
      
      throw InvalidOperationMode(ERS_HERE, get_name(), operation_mode_);
    }
  }

  virtual void setup(const size_t eventId) { ERS_INFO("Setup ... " << eventId); }

  virtual KeyedDataBlock read(const StorageKey& key)
  {
    TLOG(TLVL_DEBUG) << get_name() << ": going to read data block from triggerNumber/detectorType/apaNumber/linkNumber "
                     << HDF5KeyTranslator::get_path_string(key, config_params_.file_layout_parameters) << " from file "
                     << getFileNameFromKey(key);

    // opening the file from Storage Key + path_ + fileName_ + operation_mode_
    //std::string fullFileName = getFileNameFromKey(key);
    std::string fullFileName = HDF5KeyTranslator::get_file_name(key, config_params_, file_count_);

    // filePtr will be the handle to the Opened-File after a call to openFileIfNeeded()
    openFileIfNeeded(fullFileName, HighFive::File::ReadOnly);

    std::vector<std::string> group_and_dataset_path_elements =
      HDF5KeyTranslator::get_path_elements(key, config_params_.file_layout_parameters);


    //const std::string datasetName = std::to_string(key.getLinkNumber());
    const std::string datasetName = group_and_dataset_path_elements[3];


    KeyedDataBlock dataBlock(key);

    HighFive::Group theGroup = HDF5FileUtils::getSubGroup(filePtr, group_and_dataset_path_elements, false);

    try { // to determine if the dataset exists in the group and copy it to membuffer
      HighFive::DataSet theDataSet = theGroup.getDataSet(datasetName);
      dataBlock.data_size = theDataSet.getStorageSize();
      HighFive::DataSpace thedataSpace = theDataSet.getSpace();
      char* membuffer = new char[dataBlock.data_size];
      theDataSet.read(membuffer);
      std::unique_ptr<char> memPtr(membuffer);
      dataBlock.owned_data_start = std::move(memPtr);
    } catch (HighFive::DataSetException const&) {
      ERS_INFO("HDF5DataSet " << datasetName << " not found.");
    }
         
    return dataBlock;
  }

  /**
   * @brief HDF5DataStore write()
   * Method used to write constant data
   * into HDF5 format. Operational mode
   * defined in the configuration file.
   *
   */
  virtual void write(const KeyedDataBlock& dataBlock)
  {
    // opening the file from Storage Key + path_ + fileName_ + operation_mode_
    std::string fullFileName = HDF5KeyTranslator::get_file_name(dataBlock.data_key, config_params_, file_count_);

    // filePtr will be the handle to the Opened-File after a call to openFileIfNeeded()
    openFileIfNeeded(fullFileName, HighFive::File::OpenOrCreate);

    TLOG(TLVL_DEBUG) << get_name() << ": Writing data with run number " << dataBlock.data_key.getRunNumber()
                     << " and trigger number " << dataBlock.data_key.getTriggerNumber()
                     << " and detector type " << dataBlock.data_key.getDetectorType()
                     << " and apa/link number " << dataBlock.data_key.getApaNumber()
                     << " / " << dataBlock.data_key.getLinkNumber();

    std::vector<std::string> group_and_dataset_path_elements =
      HDF5KeyTranslator::get_path_elements(dataBlock.data_key, config_params_.file_layout_parameters);

    const std::string dataset_name = group_and_dataset_path_elements.back();
    //const std::string trh_dataset_name = "TriggerRecordHeader";

    HighFive::Group subGroup = HDF5FileUtils::getSubGroup(filePtr, group_and_dataset_path_elements, true);

    // Create dataset
    HighFive::DataSpace theDataSpace = HighFive::DataSpace({ dataBlock.data_size, 1 });
    HighFive::DataSetCreateProps dataCProps_;
    HighFive::DataSetAccessProps dataAProps_;

    auto theDataSet = subGroup.createDataSet<char>(dataset_name, theDataSpace, dataCProps_, dataAProps_);
    if (theDataSet.isValid()) {
      theDataSet.write_raw(static_cast<const char*>(dataBlock.getDataStart()));
    } else {
      throw InvalidHDF5Dataset(ERS_HERE, get_name(), dataset_name, filePtr->getName());
    } 


    filePtr->flush();
    recorded_size_ += dataBlock.data_size; 

    //AAA: be careful on the units, maybe force it somehow? 
    if (recorded_size_ > max_file_size_) {
      file_count_++;
    }
  }




  /**
   * @brief HDF5DataStore getAllExistingKeys
   * Method used to retrieve a vector with all
   * the StorageKeys
   *
   */
  virtual std::vector<StorageKey> getAllExistingKeys() const
  {
    std::vector<StorageKey> keyList;
    std::vector<std::string> fileList = getAllFiles_();

    for (auto& filename : fileList) {
      std::unique_ptr<HighFive::File> localFilePtr(new HighFive::File(filename, HighFive::File::ReadOnly));
      TLOG(TLVL_DEBUG) << get_name() << ": Opened HDF5 file " << filename;

      std::vector<std::string> pathList = HDF5FileUtils::getAllDataSetPaths(*localFilePtr);
      TLOG(TLVL_DEBUG) << get_name() << ": Path list has element count: " << pathList.size();

      for (auto& path : pathList) {
        StorageKey thisKey(0, 0, "", 0, 0);
        thisKey = HDF5KeyTranslator::getKeyFromString(path);
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

  std::unique_ptr<HighFive::File> filePtr;

  std::string path_;
  std::string fileName_;
  std::string operation_mode_;
  std::string fullNameOfOpenFile_;
  size_t max_file_size_;

  // Total number of generated files
  size_t file_count_;

  // Total size of data being written
  size_t recorded_size_;

  unsigned openFlagsOfOpenFile_;

  // Configuration
  hdf5datastore::ConfParams config_params_;

  // 31-Dec-2020, KAB: this private method is deprecated; it is moving to
  // HDF5KeyTranslator::get_file_name().
  std::string getFileNameFromKey(const StorageKey& data_key)
  {
    size_t trigger_number = data_key.getTriggerNumber();
    size_t apa_number = data_key.getApaNumber();
    std::string file_name = std::string("");
    if (operation_mode_ == "one-event-per-file") {

      file_name = path_ + "/" + fileName_ + "_trigger_number_" + std::to_string(trigger_number) + ".hdf5";

    } else if (operation_mode_ == "one-fragment-per-file") {

      file_name =
        path_ + "/" + fileName_ + "_trigger_number_" + std::to_string(trigger_number) + "_apa_number_" + std::to_string(apa_number) + ".hdf5";

    } else if (operation_mode_ == "all-per-file") {

      //file_name = path_ + "/" + fileName_ + "_all_events" + ".hdf5";
      file_name = path_ + "/" + fileName_ + "_trigger_number_" + "file_number_" + std::to_string(file_count_) + ".hdf5";
    }

    return file_name;
  }

  std::vector<std::string> getAllFiles_() const
  {
    std::string workString = fileName_;
    if (operation_mode_ == "one-event-per-file") {
      workString += "_event_\\d+.hdf5";
    } else if (operation_mode_ == "one-fragment-per-file") {
      workString += "_event_\\d+_geoID_\\d+.hdf5";
    } else {
      workString += "_all_events.hdf5";
    }

    return HDF5FileUtils::getFilesMatchingPattern(path_, workString);
  }

  void openFileIfNeeded(const std::string& fileName, unsigned openFlags = HighFive::File::ReadOnly)
  {

    if (fullNameOfOpenFile_.compare(fileName) || openFlagsOfOpenFile_ != openFlags) {

      // opening file for the first time OR something changed in the name or the way of opening the file
      TLOG(TLVL_DEBUG) << get_name() << ": going to open file " << fileName << " with openFlags "
                       << std::to_string(openFlags);
      fullNameOfOpenFile_ = fileName;
      openFlagsOfOpenFile_ = openFlags;
      filePtr.reset(new HighFive::File(fileName, openFlags));
      TLOG(TLVL_DEBUG) << get_name() << "Created HDF5 file.";

    } else {

      TLOG(TLVL_DEBUG) << get_name() << ": Pointer file to  " << fullNameOfOpenFile_
                       << " was already opened with openFlags " << std::to_string(openFlagsOfOpenFile_);
    }
  }
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_HDF5DATASTORE_HPP_

// Local Variables:
// c-basic-offset: 2
// End:
