/*
 * This file is 100% generated.  Any manual edits will likely be lost.
 *
 * This contains struct and other type definitions for shema in 
 * namespace dunedaq::dfmodules::datawriter.
 */
#ifndef DUNEDAQ_DFMODULES_DATAWRITER_STRUCTS_HPP
#define DUNEDAQ_DFMODULES_DATAWRITER_STRUCTS_HPP

#include <cstdint>

#include <string>

namespace dunedaq::dfmodules::datawriter {

    // @brief String used to specify a directory path
    using DirectoryPath = std::string;

    // @brief String used to specify a data storage operation mode
    using OperationMode = std::string;

    // @brief DataWriter configuration
    struct ConfParams {

        // @brief Path of directory where files are located
        DirectoryPath directory_path = ".";

        // @brief The operation mode that the DataStore should use when organizing the data into files
        OperationMode mode = "all-per-file";
    };

} // namespace dunedaq::dfmodules::datawriter

#endif // DUNEDAQ_DFMODULES_DATAWRITER_STRUCTS_HPP