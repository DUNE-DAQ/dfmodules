/**
 * @file TriggerRecordBuilderData.hpp TriggerRecordBuilderData Class
 *
 * The TriggerRecordBuilderData class represents the current state of a TriggerRecordBuilder's Trigger Record buffers
 * for use by the DFO.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DFMODULES_TRIGGERRECORDBUILDERDATA_HPP_
#define DFMODULES_SRC_DFMODULES_TRIGGERRECORDBUILDERDATA_HPP_

#include "daqdataformats/Types.hpp"
#include "dfmessages/TriggerDecision.hpp"

#include "ers/Issue.hpp"
#include "nlohmann/json.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>

namespace dunedaq {
ERS_DECLARE_ISSUE(dfmodules,
                  AssignedTriggerDecisionNotFound,
                  "The Trigger Decision with trigger number "
                    << trigger_number << " was not found for dataflow application at " << connection_name,
                  ((daqdataformats::trigger_number_t)trigger_number)((std::string)connection_name))

namespace dfmodules {
struct AssignedTriggerDecision
{
  dfmessages::TriggerDecision decision;
  std::chrono::steady_clock::time_point assigned_time;
  std::string connection_name;

  AssignedTriggerDecision(dfmessages::TriggerDecision dec, std::string conn_name)
    : decision(dec)
    , assigned_time(std::chrono::steady_clock::now())
    , connection_name(conn_name)
  {}
};

class TriggerRecordBuilderData
{
public:
  TriggerRecordBuilderData() = default;
  TriggerRecordBuilderData(std::string connection_name, size_t capacity);

  TriggerRecordBuilderData(TriggerRecordBuilderData const&) = delete;
  TriggerRecordBuilderData(TriggerRecordBuilderData&&);
  TriggerRecordBuilderData& operator=(TriggerRecordBuilderData const&) = delete;
  TriggerRecordBuilderData& operator=(TriggerRecordBuilderData&&);

  bool has_slot() const { return !m_in_error && m_num_slots.load() > m_assigned_trigger_decisions.size(); }
  size_t available_slots() const { return m_in_error ? 0 : m_num_slots.load() - m_assigned_trigger_decisions.size(); }

  std::shared_ptr<AssignedTriggerDecision> get_assignment(daqdataformats::trigger_number_t trigger_number) const;
  std::shared_ptr<AssignedTriggerDecision> extract_assignment(daqdataformats::trigger_number_t trigger_number);
  std::shared_ptr<AssignedTriggerDecision> make_assignment(dfmessages::TriggerDecision decision);
  void add_assignment(std::shared_ptr<AssignedTriggerDecision> assignment);
  void complete_assignment(daqdataformats::trigger_number_t trigger_number,
                           std::function<void(nlohmann::json&)> metadata_fun = nullptr);

  std::chrono::microseconds average_latency(std::chrono::steady_clock::time_point since) const;

  bool is_in_error() const { return m_in_error.load(); }
  void set_in_error(bool err) { m_in_error = err; }

private:
  std::atomic<size_t> m_num_slots;
  std::list<std::shared_ptr<AssignedTriggerDecision>> m_assigned_trigger_decisions;
  mutable std::mutex m_assigned_trigger_decisions_mutex;

  // TODO: Eric Flumerfelt <eflumerf@github.com> 03-Dec-2021: Replace with circular buffer
  std::list<std::pair<std::chrono::steady_clock::time_point, std::chrono::microseconds>> m_latency_info;
  mutable std::mutex m_latency_info_mutex;

  std::atomic<bool> m_in_error;

  nlohmann::json m_metadata;
  std::string m_connection_name;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_DFMODULES_TRIGGERRECORDBUILDERDATA_HPP_