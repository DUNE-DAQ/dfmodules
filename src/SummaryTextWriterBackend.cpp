/**
 * @file SummaryTextWriterBackend.cpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/SummaryTextWriterBackend.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace dunedaq::dfmodules {

void
SummaryTextWriterBackend::open(const std::string& path,
                               const rawdatafilelibs::FileMetadata& metadata,
                               const daqdataformats::SourceIDGeoIDMap& /*geo_id_map*/)
{
  m_file_name = path;
  m_record_name = rawdatafilelibs::record_type_to_string(metadata.record_type);
  if (std::filesystem::exists(path)) {
    throw CantOpenFile(ERS_HERE, m_file_name);
  }
  m_output_file.open(path, std::ios::out);
  if (!m_output_file.is_open()) {
    throw CantOpenFile(ERS_HERE, m_file_name);
  }
}

bool
SummaryTextWriterBackend::record_exists(const daqdataformats::RecordID& record_id)
{
  return m_written_records.count(record_id) != 0;
}

void
SummaryTextWriterBackend::write_record(const daqdataformats::RecordIndex& index,
                                       const rawdatafilelibs::BlobView& /*header*/,
                                       const std::vector<rawdatafilelibs::BlobView>& /*fragments*/)
{
  write_line(m_record_name + " #" + std::to_string(index.get_record_id().number));
  m_written_records.insert(index.get_record_id());
}

void
SummaryTextWriterBackend::close(const rawdatafilelibs::FileMetadata& /*final_metadata*/)
{
  if (!m_output_file.is_open()) {
    return;
  }
  m_output_file.close();
  if (m_output_file.fail()) {
    throw CantWriteToFile(ERS_HERE, m_file_name);
  }
}

void
SummaryTextWriterBackend::write_line(const std::string& line)
{
  m_output_file << line << '\n';
  if (!m_output_file.good()) {
    throw CantWriteToFile(ERS_HERE, m_file_name);
  }

  const size_t bytes_written = line.size() + 1;
  m_stats.recorded_size += bytes_written;
  m_stats.uncompressed_raw_data_size += bytes_written;
  m_stats.total_file_size += bytes_written;
}

} // namespace dunedaq::dfmodules
