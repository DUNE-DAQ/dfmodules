/**
 *
 * @file SummaryTextDataStore.hpp
 *
 * Header for an implementation of the DataStore interface which
 * writes out simple text files; intended for unit testing /
 * proof-of-concept purposes
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_SUMMARYTEXTDATASTORE_HPP_
#define DFMODULES_PLUGINS_SUMMARYTEXTDATASTORE_HPP_

#include "dfmodules/FileDataStoreImpl.hpp"

#include "appmodel/DataStoreConfTestDeriv.hpp"
#include "dfmodules/SummaryTextDataWriter.hpp"

#include <limits>
#include <memory>
#include <string>

namespace dunedaq::dfmodules {

  class SummaryTextDataStore : public FileDataStoreImpl<dfmodules::SummaryTextDataWriter> {

  public:
    explicit SummaryTextDataStore(std::string const& name,
                         std::shared_ptr<appfwk::ConfigurationManager> mcfg,
			   std::string const& writer_name) :
      FileDataStoreImpl(name, mcfg, writer_name),
      m_sds_config { mcfg ? mcfg->get_dal<appmodel::DataStoreConfTestDeriv>(name) : nullptr },
      m_derivval { m_sds_config ? m_sds_config->get_derivval() : std::numeric_limits<int>::max() }
    {}

    void open_new_file(const std::string& unique_filename) override;

    // This getter function and its underlying member serve no purpose
    // other than to demonstrate how you can add parameters specific
    // to your implementation of DataStore

    int get_derivval() const { return m_derivval; }

  private:
    const appmodel::DataStoreConfTestDeriv* m_sds_config; 
    const int m_derivval;
  };

  inline void SummaryTextDataStore::open_new_file(const std::string& unique_filename) {

    auto& file_handle {get_file_handle()};
    try {
      file_handle.reset(
			new dfmodules::SummaryTextDataWriter(unique_filename)
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

#endif // DFMODULES_PLUGINS_SUMMARYTEXTDATASTORE_HPP_

