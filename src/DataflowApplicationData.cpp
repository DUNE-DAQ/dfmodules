/**
 * @file DataflowApplicationData.cpp DataflowApplicationData Class Implementation
 *
 * The DataflowApplicationData class represents the current state of a dataflow application's Trigger Record buffers for
 * use by the DFO.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/DataflowApplicationData.hpp"

namespace dunedaq {
namespace dfmodules {

std::shared_ptr<AssignedTriggerDecision>
DataflowApplicationData::extract_assignment(daqdataformats::trigger_number_t trigger_number)
{
  std::shared_ptr<AssignedTriggerDecision> dec_ptr;
  auto lk = std::lock_guard<std::mutex>(m_assigned_trigger_decisions_mutex);
  for (auto it = m_assigned_trigger_decisions.begin(); it != m_assigned_trigger_decisions.end(); ++it) {
    if ((*it)->trigger_number == trigger_number) {
      dec_ptr = *it;
      m_assigned_trigger_decisions.erase(it);
      break;
    }
  }

  return dec_ptr;
}

std::shared_ptr<AssignedTriggerDecision>
DataflowApplicationData::get_assignment(daqdataformats::trigger_number_t trigger_number) const
{
  auto lk = std::lock_guard<std::mutex>(m_assigned_trigger_decisions_mutex);
  for (auto ptr : m_assigned_trigger_decisions) {
    if (ptr->trigger_number == trigger_number) {
      return ptr;
    }
  }

  return nullptr;
}

void
DataflowApplicationData::complete_assignment(daqdataformats::trigger_number_t trigger_number,
                                             std::function<void(nlohmann::json&)> metadata_fun)
{

  auto dec_ptr = extract_assignment(trigger_number);

  if (dec_ptr == nullptr)
    throw AssignedTriggerDecisionNotFound(ERS_HERE, trigger_number, m_connection_name);
  auto now = std::chrono::steady_clock::now();
  {
    auto lk = std::lock_guard<std::mutex>(m_latency_info_mutex);
    m_latency_info.emplace_back(now,
                                std::chrono::duration_cast<std::chrono::microseconds>(now - dec_ptr->assigned_time));

    if (m_latency_info.size() > 1000)
      m_latency_info.pop_front();
  }

  if (metadata_fun)
    metadata_fun(m_metadata);
}

std::shared_ptr<AssignedTriggerDecision>
DataflowApplicationData::add_assignment(daqdataformats::trigger_number_t trigger_number)
{
  auto lk = std::lock_guard<std::mutex>(m_assigned_trigger_decisions_mutex);
  m_assigned_trigger_decisions.push_back(std::make_shared<AssignedTriggerDecision>(trigger_number, m_connection_name));
  return m_assigned_trigger_decisions.back();
}

std::chrono::microseconds
DataflowApplicationData::average_latency(std::chrono::steady_clock::time_point since) const
{
  auto lk = std::lock_guard<std::mutex>(m_latency_info_mutex);
  std::chrono::microseconds sum;
  size_t count;
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