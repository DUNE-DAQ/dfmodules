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

#include "ddpdemo/StorageKey.hpp"

#include <ers/ers.h>

#include <limits>
#include <string>

namespace dunedaq {
namespace ddpdemo {

const int StorageKey::INVALID_EVENTID = std::numeric_limits<int>::max();
const int StorageKey::INVALID_GEOLOCATION = std::numeric_limits<int>::max();

int
StorageKey::getEventID() const
{
  return m_key.m_event_id;
}

std::string
StorageKey::getDetectorID() const
{
  return m_key.m_detector_id;
}

int
StorageKey::getGeoLocation() const
{
  return m_key.m_geoLocation;
}

} // namespace ddpdemo
} // namespace dunedaq
