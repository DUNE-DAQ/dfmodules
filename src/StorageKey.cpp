/**
 * @file StorageKey.hpp
 *
 * StorageKey class used to identify a given block of data
 *
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/StorageKey.hpp"

#include <ers/ers.h>

#include <limits>
#include <string>

namespace dunedaq {
namespace dfmodules {


const int StorageKey::INVALID_RUNNUMBER = std::numeric_limits<int>::max();
const int StorageKey::INVALID_TRIGGERNUMBER = std::numeric_limits<int>::max();
const int StorageKey::INVALID_APANUMBER = std::numeric_limits<int>::max(); // AAA:to be changed to something more reasonable, like 150
const int StorageKey::INVALID_LINKNUMBER = std::numeric_limits<int>::max(); // AAA: to be changed to something more reasonable, like 10

int
StorageKey::getRunNumber() const
{
  return m_key.m_run_number;
}

int
StorageKey::getTriggerNumber() const
{
  return m_key.m_trigger_number;
}

std::string
StorageKey::getDetectorType() const
{
  return m_key.m_detector_type;
}

int
StorageKey::getApaNumber() const
{
  return m_key.m_apa_number;
}

int
StorageKey::getLinkNumber() const
{
  return m_key.m_link_number;
}



} // namespace dfmodules
} // namespace dunedaq
