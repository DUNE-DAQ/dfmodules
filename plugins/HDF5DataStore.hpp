#ifndef DDPDEMO_SRC_HDF5DATASTORE_HPP_
#define DDPDEMO_SRC_HDF5DATASTORE_HPP_

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

#include "ddpdemo/DataStore.hpp"
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

ERS_DECLARE_ISSUE_BASE(ddpdemo,
                       InvalidOperationMode,
                       appfwk::GeneralDAQModuleIssue,
                       "Selected opearation mode \"" << selected_operation
                                                     << "\" is NOT supported. Please update the configuration file.",
                       ((std::string)name),
                       ((std::string)selected_operation))

ERS_DECLARE_ISSUE_BASE(ddpdemo,
                       InvalidHDF5Group,
                       appfwk::GeneralDAQModuleIssue,
                       "The HDF5 Group associated with name \"" << groupName << "\" is invalid. (file = " << filename
                                                                << ")",
                       ((std::string)name),
                       ((std::string)groupName)((std::string)filename))

ERS_DECLARE_ISSUE_BASE(ddpdemo,
                       InvalidHDF5Dataset,
                       appfwk::GeneralDAQModuleIssue,
                       "The HDF5 Dataset associated with name \"" << dataSet << "\" is invalid. (file = " << filename
                                                                  << ")",
                       ((std::string)name),
                       ((std::string)dataSet)((std::string)filename))

namespace ddpdemo {

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
    : DataStore( conf["name"].get<std::string>() ) 
    , fullNameOfOpenFile_("")
    , openFlagsOfOpenFile_(0)
  {
    TLOG(TLVL_DEBUG) << get_name() << ": Configuration: " << conf ; 
    
    fileName_ = conf["filename_prefix"].get<std::string>() ; 
    path_ = conf["directory_path"].get<std::string>() ;
    operation_mode_ = conf["mode"].get<std::string>() ; 

    if (operation_mode_ != "one-event-per-file" && operation_mode_ != "one-fragment-per-file" &&
        operation_mode_ != "all-per-file") {
      
      throw InvalidOperationMode(ERS_HERE, get_name(), operation_mode_);
    }
  }

  virtual void setup(const size_t eventId) { ERS_INFO("Setup ... " << eventId); }

  virtual KeyedDataBlock read(const StorageKey& key)
  {
    TLOG(TLVL_DEBUG) << get_name() << ": going to read data block from eventID/geoLocationID "
                     << HDF5KeyTranslator::getPathString(key) << " from file " << getFileNameFromKey(key);

    // opening the file from Storage Key + path_ + fileName_ + operation_mode_
    std::string fullFileName = getFileNameFromKey(key);
    // filePtr will be the handle to the Opened-File after a call to openFileIfNeeded()
    openFileIfNeeded(fullFileName, HighFive::File::ReadOnly);

    const std::string groupName = std::to_string(key.getEventID());
    const std::string datasetName = std::to_string(key.getGeoLocation());
    KeyedDataBlock dataBlock(key);

    if (!filePtr->exist(groupName)) {
      throw InvalidHDF5Group(ERS_HERE, get_name(), groupName, fullFileName);

    } else {

      HighFive::Group theGroup = filePtr->getGroup(groupName);

      if (!theGroup.isValid()) {

        throw InvalidHDF5Group(ERS_HERE, get_name(), groupName, fullFileName);

      } else {

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
      }
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

    size_t idx = dataBlock.data_key.getEventID();
    size_t geoID = dataBlock.data_key.getGeoLocation();

    // opening the file from Storage Key + path_ + fileName_ + operation_mode_
    std::string fullFileName = getFileNameFromKey(dataBlock.data_key);
    // filePtr will be the handle to the Opened-File after a call to openFileIfNeeded()
    openFileIfNeeded(fullFileName, HighFive::File::OpenOrCreate);

    TLOG(TLVL_DEBUG) << get_name() << ": Writing data with event ID " << dataBlock.data_key.getEventID()
                     << " and geolocation ID " << dataBlock.data_key.getGeoLocation();

    const std::string datagroup_name = std::to_string(idx);

    // Check if a HDF5 group exists and if not create one
    if (!filePtr->exist(datagroup_name)) {
      filePtr->createGroup(datagroup_name);
    }
    HighFive::Group theGroup = filePtr->getGroup(datagroup_name);

    if (!theGroup.isValid()) {
      throw InvalidHDF5Group(ERS_HERE, get_name(), datagroup_name, filePtr->getName());
    } else {
      const std::string dataset_name = std::to_string(geoID);
      HighFive::DataSpace theDataSpace = HighFive::DataSpace({ dataBlock.data_size, 1 });
      HighFive::DataSetCreateProps dataCProps_;
      HighFive::DataSetAccessProps dataAProps_;

      auto theDataSet = theGroup.createDataSet<char>(dataset_name, theDataSpace, dataCProps_, dataAProps_);
      if (theDataSet.isValid()) {
        theDataSet.write_raw(static_cast<const char*>(dataBlock.getDataStart()));
      } else {
        throw InvalidHDF5Dataset(ERS_HERE, get_name(), dataset_name, filePtr->getName());
      }
    }

    filePtr->flush();
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
        StorageKey thisKey(0, "", 0);
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
  unsigned openFlagsOfOpenFile_;

  std::string getFileNameFromKey(const StorageKey& data_key)
  {
    size_t idx = data_key.getEventID();
    size_t geoID = data_key.getGeoLocation();
    std::string file_name = std::string("");
    if (operation_mode_ == "one-event-per-file") {

      file_name = path_ + "/" + fileName_ + "_event_" + std::to_string(idx) + ".hdf5";

    } else if (operation_mode_ == "one-fragment-per-file") {

      file_name =
        path_ + "/" + fileName_ + "_event_" + std::to_string(idx) + "_geoID_" + std::to_string(geoID) + ".hdf5";

    } else if (operation_mode_ == "all-per-file") {

      file_name = path_ + "/" + fileName_ + "_all_events" + ".hdf5";
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

} // namespace ddpdemo
} // namespace dunedaq

#endif // DDPDEMO_SRC_HDF5DATASTORE_HPP_

// Local Variables:
// c-basic-offset: 2
// End:
