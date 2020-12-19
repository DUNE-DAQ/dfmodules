#ifndef DFMODULES_INCLUDE_DFMODULES_STORAGEKEY_HPP_
#define DFMODULES_INCLUDE_DFMODULES_STORAGEKEY_HPP_
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
namespace dfmodules {
/**
 * @brief The StorageKey class defines the container class that will give us a way
 * to group all of the parameters that identify a given block of data
 *
 */
struct Key
{
  Key(int run_number, int trigger_number, std::string detector_type,
      int apa_number, int link_number)
    : m_run_number(run_number)
    , m_trigger_number(trigger_number)
    , m_detector_type(detector_type)
    , m_apa_number(apa_number)
    , m_link_number(link_number)    
  {}

  int m_run_number;
  int m_trigger_number;
  std::string m_detector_type;
  int m_apa_number;
  int m_link_number;
};

class StorageKey
{

public:
  static const int INVALID_RUNNUMBER;
  static const int INVALID_TRIGGERNUMBER;
  inline static const std::string INVALID_DETECTORTYPE = "Invalid";
  static const int INVALID_APANUMBER;
  static const int INVALID_LINKNUMBER;

  //StorageKey(int eventID, std::string detectorID, int geoLocation)
//: m_key(eventID, eventID, detectorID, geoLocation, geoLocation)
//{}


  StorageKey(int run_number, int trigger_number, std::string detector_type,
      int apa_number, int link_number)
    : m_key(run_number, trigger_number, detector_type, apa_number, link_number)
  {}
  ~StorageKey() {}

  int getRunNumber() const;
  int getTriggerNumber() const;
  std::string getDetectorType() const;
  int getApaNumber() const;
  int getLinkNumber() const;

  //AAA: TODO delete
  int getEventID() const;
  std::string getDetectorID() const;
  int getGeoLocation() const;



private:
  Key m_key;
};


} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_INCLUDE_DFMODULES_STORAGEKEY_HPP_
