/**
 * @file DataflowApplicationData.hpp DataflowApplicationData Class
 *
 * The DataflowApplicationData class represents the current state of a dataflow application's Trigger Record buffers for
 * use by the DFO.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DFMODULES_DATAFLOWAPPLICATIONDATA_HPP_
#define DFMODULES_SRC_DFMODULES_DATAFLOWAPPLICATIONDATA_HPP_

#include "daqdataformats/Types.hpp"

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
  daqdataformats::trigger_number_t trigger_number;
  std::chrono::steady_clock::time_point assigned_time;
  std::string connection_name;

  AssignedTriggerDecision(daqdataformats::trigger_number_t trig_num, std::string conn_name)
    : trigger_number(trig_num)
    , assigned_time(std::chrono::steady_clock::now())
    , connection_name(conn_name)
  {}
};

class DataflowApplicationData
{
public:
  bool has_slot() const { return m_num_slots.load() > m_assigned_trigger_decisions.size(); }
  size_t available_slots() const { return m_num_slots.load() - m_assigned_trigger_decisions.size(); }

  std::shared_ptr<AssignedTriggerDecision> get_assignment(daqdataformats::trigger_number_t trigger_number) const;
  std::shared_ptr<AssignedTriggerDecision> extract_assignment(daqdataformats::trigger_number_t trigger_number);
  std::shared_ptr<AssignedTriggerDecision> add_assignment(daqdataformats::trigger_number_t trigger_number);
  void complete_assignment(daqdataformats::trigger_number_t trigger_number,
                           std::function<void(nlohmann::json&)> metadata_fun = nullptr);

  std::chrono::microseconds average_latency(std::chrono::steady_clock::time_point since) const;

private:
  std::atomic<size_t> m_num_slots;
  std::list<std::shared_ptr<AssignedTriggerDecision>> m_assigned_trigger_decisions;
  mutable std::mutex m_assigned_trigger_decisions_mutex;

  // TODO: Eric Flumerfelt <eflumerf@github.com> 03-Dec-2021: Replace with circular buffer
  std::list<std::pair<std::chrono::steady_clock::time_point, std::chrono::microseconds>> m_latency_info;
  mutable std::mutex m_latency_info_mutex;

  nlohmann::json m_metadata;
  std::string m_connection_name;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_DFMODULES_DATAFLOWAPPLICATIONDATA_HPP_