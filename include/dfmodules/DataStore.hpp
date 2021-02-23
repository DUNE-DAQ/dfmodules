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

#ifndef DFMODULES_INCLUDE_DFMODULES_DATASTORE_HPP_
#define DFMODULES_INCLUDE_DFMODULES_DATASTORE_HPP_

#include "dfmodules/KeyedDataBlock.hpp"

#include "appfwk/NamedObject.hpp"
#include "cetlib/BasicPluginFactory.h"
#include "cetlib/compiler_macros.h"
#include "dataformats/Types.hpp"
#include "logging/Logging.hpp"

#include "nlohmann/json.hpp"

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
#define DEFINE_DUNE_DATA_STORE(klass)                                                                                  \
  EXTERN_C_FUNC_DECLARE_START                                                                                          \
  std::unique_ptr<dunedaq::dfmodules::DataStore> make(const nlohmann::json& conf)                                      \
  {                                                                                                                    \
    return std::unique_ptr<dunedaq::dfmodules::DataStore>(new klass(conf));                                            \
  }                                                                                                                    \
  }

namespace dunedaq {

/**
 * @brief A ERS Issue for DataStore creation failure
 */
ERS_DECLARE_ISSUE(dfmodules,               ///< Namespace
                  DataStoreCreationFailed, ///< Type of the Issue
                  "Failed to create DataStore " << plugin_name << " with configuration "
                                                << conf,           ///< Log Message from the issue
                  ((std::string)plugin_name)((nlohmann::json)conf) ///< Message parameters
)


/**
 * @brief A generic ERS Issue for DataStore writing failure
 */
ERS_DECLARE_ISSUE(dfmodules,               ///< Namespace
                  DataStoreWritingFailed,  ///< Type of the Issue
                  "Failed to write data using  " << plugin_name,   ///< Log Message from the issue
                  ((std::string)plugin_name)   ///< Message parameters
)



namespace dfmodules {

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
   * @brief Writes the specified data payload into the DataStore.
   * @param data_block Data block to write.
   */
  virtual void write(const KeyedDataBlock& data_block) = 0;

  /**
   * @brief Writes the specified set of data blocks into the DataStore.
   * @param data_block_list List of data blocks to write.
   */
  virtual void write(const std::vector<KeyedDataBlock>& data_block_list) = 0;

  /**
   * @brief Returns the list of all keys that currently existing in the DataStore
   * @return list of StorageKeys
   */
  virtual std::vector<StorageKey> get_all_existing_keys() const = 0;

  virtual KeyedDataBlock read(const StorageKey& key) = 0;
  // virtual std::vector<KeyedDataBlock> read(const std::vector<StorageKey>& key) = 0;

  /**
   * @brief Informs the DataStore that writes or reads of data blocks associated
   * with the specified run number will soon be requested.
   * This allows DataStore instances to make any preparations that will be
   * beneficial in advance of the first data blocks being written or read.
   */
  virtual void prepare_for_run(dataformats::run_number_t run_number) = 0;

  /**
   * @brief Informs the DataStore that writes or reads of data blocks associated
   * with the specified run number have finished, for now.
   * This allows DataStore instances to do any cleanup or shutdown operations
   * that are useful once the writes or reads for a given run number have finished.
   */
  virtual void finish_with_run(dataformats::run_number_t run_number) = 0;

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
make_data_store(const std::string& type, const nlohmann::json& conf)
{
  static cet::BasicPluginFactory bpf("duneDataStore", "make"); // NOLINT

  std::unique_ptr<DataStore> ds;
  try {
    ds = bpf.makePlugin<std::unique_ptr<DataStore>>(type, conf);
  } catch (const cet::exception& cexpt) {
    throw DataStoreCreationFailed(ERS_HERE, type, conf, cexpt);
  }

  return ds;
}

/**
 * @brief Load a DataSrore plugin and return a unique_ptr to the contained
 * DAQModule class
 * @param json configuration for the DataStore. The json needs to contain the type
 * @return unique_ptr to created DataStore instance
 */
inline std::unique_ptr<DataStore>
make_data_store(const nlohmann::json& conf)
{
  return make_data_store(conf["type"].get<std::string>(), conf);
}

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_INCLUDE_DFMODULES_DATASTORE_HPP_
