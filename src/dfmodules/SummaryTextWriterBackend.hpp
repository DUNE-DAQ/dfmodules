/**
 * @file SummaryTextWriterBackend.hpp
 *
 * A minimal rawdatafilelibs WriterBackend that writes one simple text line per time slice /
 * trigger record to a text file. Used by SummaryTextDataStore, for testing the generic
 * FileDataStore code without HDF5.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DFMODULES_SUMMARYTEXTWRITERBACKEND_HPP_
#define DFMODULES_SRC_DFMODULES_SUMMARYTEXTWRITERBACKEND_HPP_

#include "rawdatafilelibs/Backend.hpp"
#include "rawdatafilelibs/FileMetadata.hpp"

#include "daqdataformats/RecordID.hpp"
#include "daqdataformats/RecordIndex.hpp"
#include "daqdataformats/SourceIDGeoIDMap.hpp"

#include "ers/Issue.hpp"

#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace dunedaq {
ERS_DECLARE_ISSUE(dfmodules, CantOpenFile, "Unable to open " << file_name, ((std::string)file_name))
ERS_DECLARE_ISSUE(dfmodules, CantWriteToFile, "Unable to write line to " << file_name, ((std::string)file_name))
} // namespace dunedaq

namespace dunedaq::dfmodules {

class SummaryTextWriterBackend : public rawdatafilelibs::WriterBackend
{
public:
  // Throws CantOpenFile, also if the file already exists
  void open(const std::string& path,
            const rawdatafilelibs::FileMetadata& metadata,
            const daqdataformats::SourceIDGeoIDMap& geo_id_map) override;
  bool record_exists(const daqdataformats::RecordID& record_id) override;
  void write_record(const daqdataformats::RecordIndex& index,
                    const rawdatafilelibs::BlobView& header,
                    const std::vector<rawdatafilelibs::BlobView>& fragments) override;
  rawdatafilelibs::FileStats get_stats() const override { return m_stats; }
  void close(const rawdatafilelibs::FileMetadata& final_metadata) override;

private:
  void write_line(const std::string& line);

  std::string m_file_name;
  std::string m_record_name; // "TriggerRecord" or "TimeSlice"
  std::ofstream m_output_file;
  rawdatafilelibs::FileStats m_stats;
  std::set<daqdataformats::RecordID> m_written_records;
};

} // namespace dunedaq::dfmodules

#endif // DFMODULES_SRC_DFMODULES_SUMMARYTEXTWRITERBACKEND_HPP_
