/*
 * This file is 100% generated.  Any manual edits will likely be lost.
 *
 * This contains struct and other type definitions for shema in 
 * namespace dunedaq::dfmodules::datatransfermodule.
 */
#ifndef DUNEDAQ_DFMODULES_DATATRANSFERMODULE_STRUCTS_HPP
#define DUNEDAQ_DFMODULES_DATATRANSFERMODULE_STRUCTS_HPP

#include <cstdint>

#include <string>

namespace dunedaq::dfmodules::datatransfermodule {

    // @brief A count of not too many things
    using Count = int32_t;

    // @brief Specific Data store implementation to be instantiated
    using DataStoreType = std::string;

    // @brief String to specify names for DataStores
    using DataStoreName = std::string;

    // @brief String used to specify a directory path
    using DirectoryPath = std::string;

    // @brief String used to specify a filename prefix
    using FilenamePrefix = std::string;

    // @brief String used to specify a data storage operation mode
    using OperationMode = std::string;

    // @brief DataStore configuration
    struct DataStore {

        // @brief DataStore specific implementation
        DataStoreType type = "HDF5DataStore";

        // @brief DataStore name
        DataStoreName name = "store";

        // @brief Path of directory where files are located
        DirectoryPath directory_path = ".";

        // @brief Filename prefix for the files on disk
        FilenamePrefix filename_prefix = "demo_run20201104";

        // @brief The operation mode that the DataStore should use when organizing the data into files
        OperationMode mode = "one-fragment-per-file";
    };

    // @brief DataTransferModule configuration
    struct Conf {

        // @brief Millisecs to sleep between generating data
        Count sleep_msec_while_running = 1000;

        // @brief Parameters that configure the DataStore instance from which data is read
        DataStore input_data_store_parameters = {"HDF5DataStore", "store", ".", "demo_run20201104", "one-fragment-per-file"};

        // @brief Parameters that configure the DataStore instance into which data is written
        DataStore output_data_store_parameters = {"HDF5DataStore", "store", ".", "demo_run20201104", "one-fragment-per-file"};
    };

    // @brief A count of very many things
    using Size = uint64_t;

} // namespace dunedaq::dfmodules::datatransfermodule

#endif // DUNEDAQ_DFMODULES_DATATRANSFERMODULE_STRUCTS_HPP