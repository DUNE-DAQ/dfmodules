/**
 * @file TriggerRecordBuilderData.cpp TriggerRecordBuilderData Class Implementation
 *
 * The TriggerRecordBuilderData class represents the current state of a dataflow application's Trigger Record buffers
 * for use by the DFO.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/TriggerRecordBuilderData.hpp"
#include "dfmodules/dfapplicationinfo/InfoNljs.hpp"

#include "logging/Logging.hpp"

#include <limits>
#include <memory>
#include <string>
#include <utility>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "TRBData" // NOLINT

namespace dunedaq {
namespace dfmodules {

TriggerRecordBuilderData::TriggerRecordBuilderData(std::string connection_name, size_t busy_threshold)
  : m_busy_threshold(busy_threshold)
  , m_free_threshold(busy_threshold)
  , m_is_busy(false)
  , m_in_error(false)
  , m_connection_name(connection_name)
{}

TriggerRecordBuilderData::TriggerRecordBuilderData(std::string connection_name,
                                                   size_t busy_threshold,
                                                   size_t free_threshold)
  : m_busy_threshold(busy_threshold)
  , m_free_threshold(busy_threshold)
  , m_is_busy(false)
  , m_in_error(false)
  , m_connection_name(connection_name)
{
  if (busy_threshold < free_threshold)
    throw dfmodules::DFOThresholdsNotConsistent(ERS_HERE, busy_threshold, free_threshold);
}

TriggerRecordBuilderData::TriggerRecordBuilderData(TriggerRecordBuilderData&& other)
{
  m_busy_threshold = other.m_busy_threshold.load();
  m_free_threshold = other.m_free_threshold.load();
  m_is_busy = other.m_is_busy.load();
  m_connection_name = std::move(other.m_connection_name);

  m_assigned_trigger_decisions = std::move(other.m_assigned_trigger_decisions);

  m_latency_info = std::move(other.m_latency_info);

  m_metadata = std::move(other.m_metadata);
  m_in_error = other.m_in_error.load();

  m_complete_counter = other.m_complete_counter.load();
  m_complete_microsecond = other.m_complete_microsecond.load();
}

TriggerRecordBuilderData&
TriggerRecordBuilderData::operator=(TriggerRecordBuilderData&& other)
{
  m_busy_threshold = other.m_busy_threshold.load();
  m_free_threshold = other.m_free_threshold.load();
  m_is_busy = other.m_is_busy.load();
  m_connection_name = std::move(other.m_connection_name);

  m_assigned_trigger_decisions = std::move(other.m_assigned_trigger_decisions);

  m_latency_info = std::move(other.m_latency_info);

  m_metadata = std::move(other.m_metadata);
  m_in_error = other.m_in_error.load();

  m_complete_counter = other.m_complete_counter.load();
  m_complete_microsecond = other.m_complete_microsecond.load();

  return *this;
}

std::shared_ptr<AssignedTriggerDecision>
TriggerRecordBuilderData::extract_assignment(daqdataformats::trigger_number_t trigger_number)
{
  std::shared_ptr<AssignedTriggerDecision> dec_ptr;
  auto lk = std::lock_guard<std::mutex>(m_assigned_trigger_decisions_mutex);
  for (auto it = m_assigned_trigger_decisions.begin(); it != m_assigned_trigger_decisions.end(); ++it) {
    if ((*it)->decision.trigger_number == trigger_number) {
      dec_ptr = *it;
      m_assigned_trigger_decisions.erase(it);
      break;
    }
  }

  if (m_assigned_trigger_decisions.size() < m_free_threshold.load())
    m_is_busy.store(false);

  return dec_ptr;
}

std::shared_ptr<AssignedTriggerDecision>
TriggerRecordBuilderData::get_assignment(daqdataformats::trigger_number_t trigger_number) const
{
  auto lk = std::lock_guard<std::mutex>(m_assigned_trigger_decisions_mutex);
  for (auto ptr : m_assigned_trigger_decisions) {
    if (ptr->decision.trigger_number == trigger_number) {
      return ptr;
    }
  }

  return nullptr;
}

std::shared_ptr<AssignedTriggerDecision>
TriggerRecordBuilderData::complete_assignment(daqdataformats::trigger_number_t trigger_number,
                                              std::function<void(nlohmann::json&)> metadata_fun)
{

  auto dec_ptr = extract_assignment(trigger_number);

  if (dec_ptr == nullptr)
    throw AssignedTriggerDecisionNotFound(ERS_HERE, trigger_number, m_connection_name);

  auto now = std::chrono::steady_clock::now();
  auto time = std::chrono::duration_cast<std::chrono::microseconds>(now - dec_ptr->assigned_time);
  {
    auto lk = std::lock_guard<std::mutex>(m_latency_info_mutex);
    m_latency_info.emplace_back(now, time);

    if (m_latency_info.size() > 1000)
      m_latency_info.pop_front();
  }

  if (metadata_fun)
    metadata_fun(m_metadata);

  ++m_complete_counter;
  auto completion_time =
    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - dec_ptr->assigned_time);
  m_complete_microsecond += completion_time.count();
  if (completion_time.count() < m_min_complete_time.load())
    m_min_complete_time.store(completion_time.count());
  if (completion_time.count() > m_max_complete_time.load())
    m_max_complete_time.store(completion_time.count());

  return dec_ptr;
}

std::list<std::shared_ptr<AssignedTriggerDecision>>
TriggerRecordBuilderData::flush()
{

  auto lk = std::lock_guard<std::mutex>(m_assigned_trigger_decisions_mutex);
  std::list<std::shared_ptr<AssignedTriggerDecision>> ret;

  for (const auto& td : m_assigned_trigger_decisions) {
    ret.push_back(td);
  }
  m_assigned_trigger_decisions.clear();

  auto stat_lock = std::lock_guard<std::mutex>(m_latency_info_mutex);
  m_latency_info.clear();
  m_is_busy = false;

  m_in_error = false;
  m_metadata = nlohmann::json();

  return ret;
}

std::shared_ptr<AssignedTriggerDecision>
TriggerRecordBuilderData::make_assignment(dfmessages::TriggerDecision decision)
{
  return std::make_shared<AssignedTriggerDecision>(decision, m_connection_name);
}

void
TriggerRecordBuilderData::add_assignment(std::shared_ptr<AssignedTriggerDecision> assignment)
{
  auto lk = std::lock_guard<std::mutex>(m_assigned_trigger_decisions_mutex);

  if (is_in_error())
    throw NoSlotsAvailable(ERS_HERE, assignment->decision.trigger_number, m_connection_name);

  m_assigned_trigger_decisions.push_back(assignment);
  TLOG_DEBUG(13) << "Size of assigned_trigger_decision list is " << m_assigned_trigger_decisions.size();

  if (m_assigned_trigger_decisions.size() >= m_busy_threshold.load()) {
    m_is_busy.store(true);
  }
}

void
TriggerRecordBuilderData::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  dfapplicationinfo::Info info;
  
  // fill metrics for complete TDs
  info.completed_trigger_records = m_complete_counter.exchange(0);
  info.waiting_time = m_complete_microsecond.exchange(0);
  info.min_completion_time = m_min_complete_time.exchange(std::numeric_limits<int64_t>::max());
  info.max_completion_time = m_max_complete_time.exchange(0);

  // fill metrics for pending TDs
  info.min_time_since_assignment = std::numeric_limits<decltype(info.min_time_since_assignment)>::max();
  info.max_time_since_assignment = 0;
  info.total_time_since_assignment = 0;

  auto lk = std::lock_guard<std::mutex>(m_assigned_trigger_decisions_mutex);

  info.outstanding_decisions = m_assigned_trigger_decisions.size();
  auto current_time = std::chrono::steady_clock::now();
  for (const auto& dec_ptr : m_assigned_trigger_decisions) {
    auto us_since_assignment =
      std::chrono::duration_cast<std::chrono::microseconds>(current_time - dec_ptr->assigned_time);
    info.total_time_since_assignment += us_since_assignment.count();
    if (us_since_assignment.count() < info.min_time_since_assignment)
      info.min_time_since_assignment = us_since_assignment.count();
    if (us_since_assignment.count() > info.max_time_since_assignment)
      info.max_time_since_assignment = us_since_assignment.count();
  }

  if ( info.completed_trigger_records > 0 ) {
    m_last_average_time = 1e-6*info.waiting_time/info.completed_trigger_records ;
  }
  
  // prediction rate metrics
  info.capacity_rate = m_busy_threshold.load()/m_last_average_time;
 
  ci.add(info);
}

std::chrono::microseconds
TriggerRecordBuilderData::average_latency(std::chrono::steady_clock::time_point since) const
{
  auto lk = std::lock_guard<std::mutex>(m_latency_info_mutex);
  std::chrono::microseconds sum = std::chrono::microseconds(0);
  size_t count = 0;
  for (auto it = m_latency_info.rbegin(); it != m_latency_info.rend(); ++it) {
    if (it->first < since)
      break;

    count++;
    sum += it->second;
  }

  return sum / count;
}

} // namespace dfmodules
} // namespace dunedaq
