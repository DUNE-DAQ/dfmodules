/**
 * @file DFOCore.cpp  DFOCore class implementation.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/DFOCore.hpp"

#include <chrono>
#include <limits>
#include <list>
#include <memory>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <utility>

/**
 * @brief Name used by TRACE TLOG calls from this source file
 */
#define TRACE_NAME "DFOCore" // NOLINT
enum
{
  TLVL_CONFIG = 7,
  TLVL_WORK_STEPS = 10,
  TLVL_TRIGDEC_RECEIVED = 21,
  TLVL_NOTIFY_TRIGGER = 22,
  TLVL_DISPATCH_TO_TRB = 23,
  TLVL_TDTOKEN_RECEIVED = 24
};

namespace dunedaq::dfmodules {

DFOCore::DFOCore(std::string owner_name)
  : m_owner_name(std::move(owner_name))
  , m_last_assignment_it(m_dataflow_availability.end())
{}

void
DFOCore::configure(size_t busy_threshold,
                   size_t free_threshold,
                   size_t td_send_retries,
                   std::chrono::milliseconds queue_timeout,
                   std::chrono::microseconds stop_timeout)
{
  m_busy_threshold = busy_threshold;
  m_free_threshold = free_threshold;
  m_td_send_retries = td_send_retries;
  m_queue_timeout = queue_timeout;
  m_stop_timeout = stop_timeout;
}

void
DFOCore::start(daqdataformats::run_number_t run_number,
               std::shared_ptr<iomanager::SenderConcept<dfmessages::TriggerInhibit>> busy_sender,
               td_sender_fn_t get_td_sender_fn,
               new_trb_fn_t on_new_trb_fn)
{
  m_run_number = run_number;
  m_busy_sender = std::move(busy_sender);
  m_get_td_sender_fn = std::move(get_td_sender_fn);
  m_on_new_trb_fn = std::move(on_new_trb_fn);

  m_received_tokens.store(0);
  m_sent_decisions.store(0);
  m_received_decisions.store(0);
  m_waiting_for_decision.store(0);
  m_deciding_destination.store(0);
  m_forwarding_decision.store(0);
  m_waiting_for_token.store(0);
  m_processing_token.store(0);

  m_running_status.store(true);
  m_last_notified_busy.store(false);
  m_last_assignment_it = m_dataflow_availability.end();
  m_last_token_received = m_last_td_received = std::chrono::steady_clock::now();
}

std::list<std::shared_ptr<AssignedTriggerDecision>>
DFOCore::stop()
{
  m_running_status.store(false);

  const int wait_steps = 20;
  auto step_timeout = m_stop_timeout / wait_steps;
  int step_counter = 0;
  while (!is_empty() && step_counter < wait_steps) {
    TLOG() << m_owner_name << ": stop delayed while waiting for " << used_slots() << " TDs to complete";
    std::this_thread::sleep_for(step_timeout);
    ++step_counter;
  }

  std::list<std::shared_ptr<AssignedTriggerDecision>> remnants;
  for (auto& app : m_dataflow_availability) {
    for (auto& td : app.second->flush()) {
      remnants.push_back(td);
    }
  }

  std::lock_guard<std::mutex> guard(m_trigger_counters_mutex);
  m_trigger_counters.clear();

  return remnants;
}

void
DFOCore::scrap()
{
  m_dataflow_availability.clear();
  m_get_td_sender_fn = nullptr;
  m_on_new_trb_fn = nullptr;
  m_busy_sender.reset();
}

void
DFOCore::receive_token(const dfmessages::TriggerDecisionToken& token)
{
  if (token.run_number == 0 && token.trigger_number == 0) {
    if (m_dataflow_availability.count(token.decision_destination) == 0) {
      TLOG_DEBUG(TLVL_CONFIG) << "Creating dataflow availability struct for uid " << token.decision_destination;
      auto entry = m_dataflow_availability[token.decision_destination] =
        std::make_shared<TriggerRecordBuilderData>(token.decision_destination, m_busy_threshold, m_free_threshold);
      if (m_on_new_trb_fn) {
        m_on_new_trb_fn(token.decision_destination, entry);
      }
    } else {
      TLOG() << TRBModuleAppUpdate(ERS_HERE, token.decision_destination, "Has reconnected");
      m_dataflow_availability[token.decision_destination]->set_in_error(false);
    }
    return;
  }

  TLOG_DEBUG(TLVL_TDTOKEN_RECEIVED) << m_owner_name << " Received TriggerDecisionToken for trigger_number "
                                    << token.trigger_number << " and run " << token.run_number
                                    << " (current run is " << m_run_number << ")";

  if (token.run_number != m_run_number) {
    std::ostringstream oss_source;
    oss_source << "TRB at connection " << token.decision_destination;
    ers::error(DFOModuleRunNumberMismatch(
      ERS_HERE, token.run_number, m_run_number, oss_source.str(), token.trigger_number));
    return;
  }

  auto app_it = m_dataflow_availability.find(token.decision_destination);
  if (app_it == m_dataflow_availability.end()) {
    ers::error(UnknownTokenSource(ERS_HERE, token.decision_destination));
    return;
  }

  ++m_received_tokens;
  auto callback_start = std::chrono::steady_clock::now();

  try {
    auto dec_ptr = app_it->second->complete_assignment(token.trigger_number, m_metadata_function);
    for (const auto t : unpack_types(dec_ptr->decision.trigger_type)) {
      ++get_trigger_counter(t).completed;
    }
  } catch (AssignedTriggerDecisionNotFound const& err) {
    ers::error(err);
  }

  if (app_it->second->is_in_error()) {
    TLOG() << TRBModuleAppUpdate(ERS_HERE, token.decision_destination, "Has reconnected");
    app_it->second->set_in_error(false);
  }

  notify_trigger_if_needed();

  m_waiting_for_token +=
    std::chrono::duration_cast<std::chrono::microseconds>(callback_start - m_last_token_received).count();
  m_last_token_received = std::chrono::steady_clock::now();
  m_processing_token +=
    std::chrono::duration_cast<std::chrono::microseconds>(m_last_token_received - callback_start).count();
}

void
DFOCore::receive_trigger_decision(const dfmessages::TriggerDecision& decision)
{
  TLOG_DEBUG(TLVL_TRIGDEC_RECEIVED) << m_owner_name << " Received TriggerDecision for trigger_number "
                                    << decision.trigger_number << " and run " << decision.run_number
                                    << " (current run is " << m_run_number << ")";

  if (decision.run_number != m_run_number) {
    ers::error(DFOModuleRunNumberMismatch(
      ERS_HERE, decision.run_number, m_run_number, "MLT", decision.trigger_number));
    return;
  }

  auto decision_received = std::chrono::steady_clock::now();
  ++m_received_decisions;
  for (const auto t : unpack_types(decision.trigger_type)) {
    ++get_trigger_counter(t).received;
  }

  std::chrono::steady_clock::time_point decision_assigned;
  do {
    auto assignment = find_slot(decision);

    if (assignment == nullptr) { // all applications may be in error state
      ers::error(UnableToAssign(ERS_HERE, decision.trigger_number));
      usleep(500);
      notify_trigger_if_needed();
      continue;
    }

    TLOG_DEBUG(TLVL_TRIGDEC_RECEIVED) << m_owner_name << " Slot found for trigger_number " << decision.trigger_number
                                      << " on connection " << assignment->connection_name
                                      << ", number of used slots is " << used_slots();
    decision_assigned = std::chrono::steady_clock::now();
    auto dispatch_successful = dispatch(assignment);

    if (dispatch_successful) {
      assign_trigger_decision(assignment);
      TLOG_DEBUG(TLVL_TRIGDEC_RECEIVED) << m_owner_name << " Assigned trigger_number " << decision.trigger_number
                                        << " to connection " << assignment->connection_name;
      break;
    } else {
      ers::error(TRBModuleAppUpdate(ERS_HERE, assignment->connection_name, "Could not send Trigger Decision"));
      m_dataflow_availability[assignment->connection_name]->set_in_error(true);
    }

  } while (m_running_status.load());

  notify_trigger_if_needed();

  m_waiting_for_decision +=
    std::chrono::duration_cast<std::chrono::microseconds>(decision_received - m_last_td_received).count();
  m_last_td_received = std::chrono::steady_clock::now();
  m_deciding_destination +=
    std::chrono::duration_cast<std::chrono::microseconds>(decision_assigned - decision_received).count();
  m_forwarding_decision +=
    std::chrono::duration_cast<std::chrono::microseconds>(m_last_td_received - decision_assigned).count();
}

bool
DFOCore::is_busy() const
{
  for (auto& dfapp : m_dataflow_availability) {
    if (!dfapp.second->is_busy())
      return false;
  }
  return true;
}

bool
DFOCore::is_empty() const
{
  for (auto& dfapp : m_dataflow_availability) {
    if (dfapp.second->used_slots() != 0)
      return false;
  }
  return true;
}

size_t
DFOCore::used_slots() const
{
  size_t total = 0;
  for (auto& dfapp : m_dataflow_availability) {
    total += dfapp.second->used_slots();
  }
  return total;
}

void
DFOCore::notify_trigger_if_needed()
{
  // Combine is_busy() check and send in a single mutex-protected block to
  // avoid a race in which the busy state changes between check and send.
  std::lock_guard<std::mutex> guard(m_notify_trigger_mutex);

  bool busy = is_busy();
  if (busy == m_last_notified_busy.load())
    return;

  bool wasSentSuccessfully = false;
  do {
    try {
      dfmessages::TriggerInhibit message{ busy, m_run_number };
      m_busy_sender->send(std::move(message), m_queue_timeout);
      wasSentSuccessfully = true;
      TLOG_DEBUG(TLVL_NOTIFY_TRIGGER) << m_owner_name << " Sent BUSY status " << busy << " to trigger in run "
                                      << m_run_number;
    } catch (const ers::Issue& excpt) {
      ers::warning(excpt);
    }
  } while (!wasSentSuccessfully && m_running_status.load());

  m_last_notified_busy.store(busy);
}

DFOCore::OpMonSnapshot
DFOCore::take_opmon_snapshot()
{
  return { m_received_tokens.exchange(0),
           m_received_decisions.exchange(0),
           m_sent_decisions.exchange(0),
           m_waiting_for_decision.exchange(0),
           m_deciding_destination.exchange(0),
           m_forwarding_decision.exchange(0),
           m_waiting_for_token.exchange(0),
           m_processing_token.exchange(0) };
}

std::shared_ptr<AssignedTriggerDecision>
DFOCore::find_slot(const dfmessages::TriggerDecision& decision)
{
  // Round-robin across all available apps; apps in error are skipped.
  // If all are busy, assign to the one with the fewest used slots.
  // Returning nullptr is treated as an error by the caller.

  std::shared_ptr<AssignedTriggerDecision> output = nullptr;
  auto minimum_occupied = m_dataflow_availability.end();
  size_t minimum = std::numeric_limits<size_t>::max();
  unsigned int counter = 0;

  auto candidate_it = m_last_assignment_it;
  if (candidate_it == m_dataflow_availability.end())
    candidate_it = m_dataflow_availability.begin();

  while (output == nullptr && counter < m_dataflow_availability.size()) {
    ++counter;
    ++candidate_it;
    if (candidate_it == m_dataflow_availability.end())
      candidate_it = m_dataflow_availability.begin();

    if (candidate_it->second->is_in_error())
      continue;

    auto slots = candidate_it->second->used_slots();
    if (slots < minimum) {
      minimum = slots;
      minimum_occupied = candidate_it;
    }

    if (candidate_it->second->is_busy())
      continue;

    output = candidate_it->second->make_assignment(decision);
    m_last_assignment_it = candidate_it;
  }

  if (!output) {
    if (minimum_occupied != m_dataflow_availability.end()) {
      output = minimum_occupied->second->make_assignment(decision);
      m_last_assignment_it = minimum_occupied;
      ers::warning(AssignedToBusyApp(ERS_HERE, decision.trigger_number, minimum_occupied->first, minimum));
    }
  }

  if (output != nullptr) {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "Assigned TriggerDecision with trigger number " << decision.trigger_number
                                << " to TRB at connection " << output->connection_name;
  }
  return output;
}

bool
DFOCore::dispatch(const std::shared_ptr<AssignedTriggerDecision>& assignment)
{
  TLOG_DEBUG(5) << m_owner_name << ": Entering dispatch(). assignment->connection_name: "
                << assignment->connection_name;

  bool wasSentSuccessfully = false;
  int retries = static_cast<int>(m_td_send_retries);
  do {
    try {
      auto decision_copy = dfmessages::TriggerDecision(assignment->decision);
      m_get_td_sender_fn(assignment->connection_name)->send(std::move(decision_copy), m_queue_timeout);
      wasSentSuccessfully = true;
      ++m_sent_decisions;
      TLOG_DEBUG(TLVL_DISPATCH_TO_TRB) << m_owner_name << " Sent TriggerDecision for trigger_number "
                                       << assignment->decision.trigger_number << " to TRB at connection "
                                       << assignment->connection_name << " for run number "
                                       << assignment->decision.run_number;
    } catch (const ers::Issue& excpt) {
      std::ostringstream oss_warn;
      oss_warn << "Send to connection \"" << assignment->connection_name << "\" failed";
      ers::warning(excpt);
    }
    --retries;
  } while (!wasSentSuccessfully && m_running_status.load() && retries > 0);

  return wasSentSuccessfully;
}

void
DFOCore::assign_trigger_decision(const std::shared_ptr<AssignedTriggerDecision>& assignment)
{
  m_dataflow_availability[assignment->connection_name]->add_assignment(assignment);
}

DFOCore::TriggerData&
DFOCore::get_trigger_counter(trgdataformats::TriggerCandidateData::Type type)
{
  auto it = m_trigger_counters.find(type);
  if (it != m_trigger_counters.end())
    return it->second;

  std::lock_guard<std::mutex> guard(m_trigger_counters_mutex);
  return m_trigger_counters[type];
}

} // namespace dunedaq::dfmodules
