/**
 * @file DFODecision.hpp  DFODecision message used by DFOConsensusModule.
 *
 * A DFODecision is broadcast by the responsible DFOConsensusModule to all
 * peer DFOs after each TRBModule state change (trigger assignment or
 * completion).  This allows every DFO in the ensemble to maintain an accurate
 * per-TRBModule slot-usage view and issue correct TriggerInhibit messages to
 * the MLT.
 *
 * Fields
 * ------
 * run_number         – run in which this decision was made
 * trigger_number     – the TriggerDecision that triggered the state change
 * trb_connection_name– connection-ID of the TRBModule whose slot count changed
 * trb_slot_count     – absolute slot count for that TRB *after* the change
 * source_dfo_name    – name() of the DFO that generated this message
 * is_completion      – false = new assignment; true = TRB completed a trigger
 *
 * Serialisation
 * -------------
 * nlohmann/json (to_json / from_json) is provided via
 * NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE.  For in-process Queue transport this is
 * not required; for network (ZMQ / kSendRecv) connections production
 * deployments should additionally register a msgpack serialiser in dfmessages.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_INCLUDE_DFMODULES_DFODECISION_HPP_
#define DFMODULES_INCLUDE_DFMODULES_DFODECISION_HPP_

#include "daqdataformats/Types.hpp"
#include "nlohmann/json.hpp"

#include <string>

namespace dunedaq {
namespace dfmodules {

struct DFODecision
{
  daqdataformats::run_number_t run_number{ 0 };
  daqdataformats::trigger_number_t trigger_number{ 0 };
  std::string trb_connection_name;  ///< Connection-ID of the TRBModule involved
  size_t trb_slot_count{ 0 };       ///< TRB slot count after this event
  std::string source_dfo_name;      ///< Name of the DFO that generated this message
  bool is_completion{ false };      ///< true = completion; false = new assignment
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DFODecision,
                                   run_number,
                                   trigger_number,
                                   trb_connection_name,
                                   trb_slot_count,
                                   source_dfo_name,
                                   is_completion)

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_INCLUDE_DFMODULES_DFODECISION_HPP_
