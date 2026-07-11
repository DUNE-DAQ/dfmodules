/**
 * @file StubDataWriter.cpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/StubDataWriter.hpp"

#include <stdexcept>
#include <string>

namespace dunedaq::dfmodules {

StubDataWriter::StubDataWriter(const std::string& file_name)
  : m_file_name(file_name)
  , m_output_file(file_name, std::ios::out | std::ios::app)
  , m_recorded_size(0)
  , m_uncompressed_raw_data_size(0)
  , m_total_file_size(0)
{
  if (!m_output_file.is_open()) {
    throw CantOpenFile(ERS_HERE, m_file_name);
  }
}

StubDataWriter::~StubDataWriter()
{
  if (m_output_file.is_open()) {
    m_output_file.close();
  }
}

void
StubDataWriter::write(const daqdataformats::TriggerRecord& tr)
{
  const auto trigger_number = tr.get_header_ref().get_trigger_number();
  write_line("TriggerRecord #" + std::to_string(trigger_number));
}

void
StubDataWriter::write(const daqdataformats::TimeSlice& ts)
{
  const auto timeslice_number = ts.get_header().timeslice_number;
  m_written_timeslices.insert(timeslice_number);
  write_line("TimeSlice #" + std::to_string(timeslice_number));
}

bool
StubDataWriter::timeslice_already_exists(const daqdataformats::TimeSlice& ts) const
{
  return m_written_timeslices.count(ts.get_header().timeslice_number) > 0;
}

std::string
StubDataWriter::get_file_name() const
{
  return m_file_name;
}

size_t
StubDataWriter::get_recorded_size() const noexcept
{
  return m_recorded_size;
}

size_t
StubDataWriter::get_uncompressed_raw_data_size() const noexcept
{
  return m_uncompressed_raw_data_size;
}

size_t
StubDataWriter::get_total_file_size() const noexcept
{
  return m_total_file_size;
}

void
StubDataWriter::write_line(const std::string& line)
{
  m_output_file << line << '\n';
  if (!m_output_file.good()) {
    throw CantWriteToFile(ERS_HERE, m_file_name);
  }

  const size_t bytes_written = line.size() + 1;
  m_recorded_size += bytes_written;
  m_uncompressed_raw_data_size += bytes_written;
  m_total_file_size += bytes_written;
}

} // namespace dunedaq::dfmodules

