#ifndef DDPDEMO_INCLUDE_DDPDEMO_STORAGEKEY_HPP_
#define DDPDEMO_INCLUDE_DDPDEMO_STORAGEKEY_HPP_
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

#include <string>

namespace dunedaq {
namespace ddpdemo {
/**
 * @brief The StorageKey class defines the container class that will give us a way
 * to group all of the parameters that identify a given block of data
 *
 */
struct Key
{
  Key(int eventID, std::string detectorID, int geoLocation)
    : m_event_id(eventID)
    , m_detector_id(detectorID)
    , m_geoLocation(geoLocation)
  {}

  int m_event_id;
  std::string m_detector_id;
  int m_geoLocation;
};

class StorageKey
{

public:
  static const int INVALID_EVENTID;
  inline static const std::string INVALID_DETECTORID = "Invalid";
  static const int INVALID_GEOLOCATION;

  StorageKey(int eventID, std::string detectorID, int geoLocation)
    : m_key(eventID, detectorID, geoLocation)
  {}
  ~StorageKey() {}

  int getEventID() const;

  std::string getDetectorID() const;

  int getGeoLocation() const;

private:
  Key m_key;
};

} // namespace ddpdemo
} // namespace dunedaq

#endif // DDPDEMO_INCLUDE_DDPDEMO_STORAGEKEY_HPP_
