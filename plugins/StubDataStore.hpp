/**
 *
 * @file StubDataStore.hpp
 *
 * Header for an implementation of the DataStore interface which
 * writes out simple text files; intended for unit testing /
 * proof-of-concept purposes
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_STUBDATASTORE_HPP_
#define DFMODULES_PLUGINS_STUBDATASTORE_HPP_

#include "dfmodules/FileDataStoreImpl.hpp"

#include "appmodel/DataStoreConf.hpp"
#include "dfmodules/StubDataWriter.hpp"

#include <string>

namespace dunedaq::dfmodules {

  class StubDataStore : public FileDataStoreImpl<dfmodules::StubDataWriter,
					     appmodel::DataStoreConf> {

  public:
    explicit StubDataStore(std::string const& name,
                         std::shared_ptr<appfwk::ConfigurationManager> mcfg,
			   std::string const& writer_name) :
      FileDataStoreImpl(name, mcfg, writer_name)
    {}

    void open_new_file(const std::string& unique_filename) override;
  };

  inline void StubDataStore::open_new_file(const std::string& unique_filename) {

    auto& file_handle {get_file_handle()};
    try {
      file_handle.reset(
			new dfmodules::StubDataWriter(unique_filename)
			);
    } catch (std::exception const& excpt) {
        throw FileOperationProblem(ERS_HERE, get_name(), unique_filename, excpt);
    } catch (...) { // NOLINT(runtime/exceptions)                                                                                             
        // NOLINT here because we *ARE* re-throwing the exception!                                                                              
        throw FileOperationProblem(ERS_HERE, get_name(), unique_filename);
    }

    TLOG_DEBUG(TLVL_BASIC) << get_name() << "Created stub file (" << unique_filename << ").";
  }
} // namespace dunedaq::dfmodules

#endif // DFMODULES_PLUGINS_STUBDATASTORE_HPP_

