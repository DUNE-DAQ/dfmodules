/**
 * @file AssignedTriggerDecision.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DFMODULES_ASSIGNEDTRIGGERDECISION_HPP_
#define DFMODULES_SRC_DFMODULES_ASSIGNEDTRIGGERDECISION_HPP_

#include "dfmessages/TriggerDecision.hpp"

#include <chrono>
#include <string>

namespace dunedaq::dfmodules {

    struct AssignedTriggerDecision
{
  dfmessages::TriggerDecision decision;
  std::chrono::steady_clock::time_point assigned_time;
  std::string connection_name;
  AssignedTriggerDecision(dfmessages::TriggerDecision dec, std::string conn_name)
    : decision(dec)
    , assigned_time(std::chrono::steady_clock::now())
    , connection_name(conn_name)
  {
  }
};

} // namespace dunedaq::dfmodules

#endif // DFMODULES_SRC_DFMODULES_ASSIGNEDTRIGGERDECISION_HPP_
