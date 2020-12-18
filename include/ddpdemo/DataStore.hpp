/**
 * @file DataStore.hpp
 *
 * This is the interface for storing and retrieving data from
 * various storage systems.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

// 09-Sep-2020, KAB: the initial version of this class was based on the
// Queue interface from the appfwk repo.

#ifndef DDPDEMO_INCLUDE_DDPDEMO_DATASTORE_HPP_
#define DDPDEMO_INCLUDE_DDPDEMO_DATASTORE_HPP_

#include "ddpdemo/KeyedDataBlock.hpp"

#include <appfwk/NamedObject.hpp>

#include <nlohmann/json.hpp>

#include "ers/ers.h"

#include <cetlib/BasicPluginFactory.h>
#include <cetlib/compiler_macros.h>
#include <memory>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#ifndef EXTERN_C_FUNC_DECLARE_START
#define EXTERN_C_FUNC_DECLARE_START                                                                                    \
  extern "C"                                                                                                           \
  {
#endif
/**
 * @brief Declare the function that will be called by the plugin loader
 * @param klass Class to be defined as a DUNE IPM Receiver
 */
#define DEFINE_DUNE_DATA_STORE(klass)                                   \
  EXTERN_C_FUNC_DECLARE_START						\
  std::unique_ptr<dunedaq::ddpdemo::DataStore> make( const nlohmann::json & conf ) \
    { return std::unique_ptr<dunedaq::ddpdemo::DataStore>( new klass(conf) ); } \
  }

namespace dunedaq {

  /**
   * @brief A ERS Issue for DataStore creation failure
   */
  ERS_DECLARE_ISSUE( ddpdemo,                                                           ///< Namespace
		     DataStoreCreationFailed,                                           ///< Type of the Issue
		     "Failed to create DataStore " << plugin_name << " with configuration " << conf, ///< Log Message from the issue
		     ((std::string)plugin_name)((nlohmann::json)conf)                        ///< Message parameters
		     )


namespace ddpdemo {



/**
 * @brief comment
 */
class DataStore : public appfwk::NamedObject
{
public:
  /**
   * @brief DataStore Constructor
   * @param name Name of the DataStore instance
   */
  explicit DataStore(const std::string& name)
    : appfwk::NamedObject(name)
  {}

  /**
   * @brief Setup the DataStore for reading/writign.
   * @param directory path and filename.
   */
  virtual void setup(const size_t eventId) = 0;

  /**
   * @brief Writes the specified data payload into the DataStore.
   * @param dataBlock Data block to write.
   */
  virtual void write(const KeyedDataBlock& dataBlock) = 0;

  /**
   * @brief Returns the list of all keys that currently existing in the DataStore
   * @return list of StorageKeys
   */
  virtual std::vector<StorageKey> getAllExistingKeys() const = 0;

  // Ideas for future work...
  // virtual void write(const std::vector<KeyedDataBlock>& dataBlockList) = 0;
  virtual KeyedDataBlock read(const StorageKey& key) = 0;
  // virtual std::vector<KeyedDataBlock> read(const std::vector<StorageKey>& key) = 0;

private:
  DataStore(const DataStore&) = delete;
  DataStore& operator=(const DataStore&) = delete;
  DataStore(DataStore&&) = default;
  DataStore& operator=(DataStore&&) = default;
};

  /**
   * @brief Load a DataSrore plugin and return a unique_ptr to the contained
   * DAQModule class
   * @param plugin_name Name of the plugin, e.g. HDF5DataStore 
   * @param json configuration for the DataStore 
   * @return unique_ptr to created DataStore instance
   */
  inline std::unique_ptr<DataStore>
  makeDataStore( const std::string & type, const nlohmann::json & conf ) {
    static cet::BasicPluginFactory bpf("duneDataStore", "make");

    std::unique_ptr<DataStore> ds ;
    try { 
      ds = bpf.makePlugin<std::unique_ptr<DataStore>>( type, conf ) ;
    }
    catch (const cet::exception& cexpt) {
      throw DataStoreCreationFailed(ERS_HERE, type, conf, cexpt);
    }
 
    return ds ;
  }


  /**
   * @brief Load a DataSrore plugin and return a unique_ptr to the contained
   * DAQModule class
   * @param json configuration for the DataStore. The json needs to contain the type
   * @return unique_ptr to created DataStore instance
   */
  inline std::unique_ptr<DataStore>
  makeDataStore( const nlohmann::json & conf ) {
    return makeDataStore( conf["type"].get<std::string>(), conf ) ; 
  }
  

  
} // namespace ddpdemo
} // namespace dunedaq



#endif // DDPDEMO_INCLUDE_DDPDEMO_DATASTORE_HPP_
