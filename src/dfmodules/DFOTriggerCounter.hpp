/**
 * @file DFOTriggerCounter.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DFMODULES_DFOTRIGGERCOUNTER_HPP_
#define DFMODULES_SRC_DFMODULES_DFOTRIGGERCOUNTER_HPP_

#include "trgdataformats/TriggerCandidateData.hpp"

#include <cstdint>
#include <atomic>
#include <bitset>
#include <set>

namespace dunedaq::dfmodules {

struct DFOTriggerCounter
{
  std::atomic<uint64_t> received{ 0 }; // NOLINT
  std::atomic<uint64_t> completed{ 0 }; // NOLINT

  static std::set<trgdataformats::TriggerCandidateData::Type> unpack_types(
    decltype(dfmessages::TriggerDecision::trigger_type) t)
  {
    std::set<trgdataformats::TriggerCandidateData::Type> results;
    if (t == dfmessages::TypeDefaults::s_invalid_trigger_type)
      return results;
    const std::bitset<64> bits(t);
    for (size_t i = 0; i < bits.size(); ++i) {
      if (bits[i])
        results.insert(static_cast<trgdataformats::TriggerCandidateData::Type>(i));
    }
    return results;
  }
};

} // namespace dunedaq::dfmodules

#endif // DFMODULES_SRC_DFMODULES_DFOTRIGGERCOUNTER_HPP_
