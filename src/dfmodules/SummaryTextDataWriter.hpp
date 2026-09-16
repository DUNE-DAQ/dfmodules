/**
 * @file SummaryTextDataWriter.hpp
 *
 * A minimal implementation of the FileHandleConcept that writes one
 * simple text line per time slice / trigger record to a text file
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DFMODULES_SUMMARYTEXTDATAWRITER_HPP_
#define DFMODULES_SRC_DFMODULES_SUMMARYTEXTDATAWRITER_HPP_

#include "daqdataformats/TimeSlice.hpp"
#include "daqdataformats/TriggerRecord.hpp"

#include "ers/Issue.hpp"

#include <fstream>
#include <set>
#include <string>

namespace dunedaq {
  ERS_DECLARE_ISSUE(dfmodules, CantOpenFile, "Unable to open " << file_name, ((std::string)file_name))
  ERS_DECLARE_ISSUE(dfmodules, CantWriteToFile, "Unable to write line to " << file_name, ((std::string)file_name))
} // namespace dunedaq

namespace dunedaq::dfmodules {

class SummaryTextDataWriter
{
public:
  explicit SummaryTextDataWriter(const std::string& file_name);

  void write(const daqdataformats::TriggerRecord& tr);
  void write(const daqdataformats::TimeSlice& ts);

  bool timeslice_already_exists(const daqdataformats::TimeSlice& ts) const;

  std::string get_file_name() const;
  std::string get_file_name_extension() const {
    return "txt";
  }
  size_t get_recorded_size() const noexcept;
  size_t get_uncompressed_raw_data_size() const noexcept;
  size_t get_total_file_size() const noexcept;

  ~SummaryTextDataWriter();

  SummaryTextDataWriter(const SummaryTextDataWriter&) = delete;
  SummaryTextDataWriter& operator=(const SummaryTextDataWriter&) = delete;
  SummaryTextDataWriter(SummaryTextDataWriter&&) = delete;
  SummaryTextDataWriter& operator=(SummaryTextDataWriter&&) = delete;
  
private:
  void write_line(const std::string& line);

  std::string m_file_name;
  std::ofstream m_output_file;
  size_t m_recorded_size;
  size_t m_uncompressed_raw_data_size;
  size_t m_total_file_size;
  std::set<daqdataformats::timeslice_number_t> m_written_timeslices;
};

} // namespace dunedaq::dfmodules

#endif // DFMODULES_SRC_DFMODULES_SUMMARYTEXTDATAWRITER_HPP_
