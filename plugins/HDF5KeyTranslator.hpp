#ifndef DDPDEMO_SRC_HDF5KEYTRANSLATOR_HPP_
#define DDPDEMO_SRC_HDF5KEYTRANSLATOR_HPP_
/**
 * @file HDF5KeyTranslator.hpp
 *
 * HDF5KeyTranslator collection of functions to translate between
 * StorageKeys and HDF5 Group/DataSet 'paths'.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "ddpdemo/StorageKey.hpp"

#include <boost/algorithm/string.hpp>

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace dunedaq {
namespace ddpdemo {

class HDF5KeyTranslator
{

public:
  inline static const std::string PATH_SEPARATOR = "/";

  static const int EVENT_ID_DIGITS = 4;
  static const int GEO_LOCATION_DIGITS = 3;

  /**
   * @brief Translates the specified StorageKey into an HDF5 'path',
   * where the 'path' is string that has values from the StorageKey
   * separated by slashes (e.g. "<eventID/geoLoc>", "0001/003", "0001/FELIX/003").
   * The intention of this path string is to specify the Group/DataSet
   * structure that should be used in the HDF5 files that are created by this library.
   */
  static std::string getPathString(const StorageKey& key)
  {
    std::vector<std::string> elementList = getPathElements(key);

    std::string path = elementList[0]; // need error checking

    for (size_t idx = 1; idx < elementList.size(); ++idx) {
      path = path + PATH_SEPARATOR + elementList[idx];
    }

    return path;
  }

  /**
   * @brief Translates the specified StorageKey into the elements of an HDF5 'path',
   * where the 'path' elements are the strings that specify the Group/DataSet
   * structure that should be used in the HDF5 files that are created by this library.
   */
  static std::vector<std::string> getPathElements(const StorageKey& key)
  {
    std::vector<std::string> elementList;

    // first, we take care of the event ID
    std::ostringstream evIdString;
    evIdString << std::setw(EVENT_ID_DIGITS) << std::setfill('0') << key.getEventID();
    elementList.push_back(evIdString.str());

    // next, we translate the geographic location
    std::ostringstream geoLocString;
    geoLocString << std::setw(GEO_LOCATION_DIGITS) << std::setfill('0') << key.getGeoLocation();
    elementList.push_back(geoLocString.str());

    return elementList;
  }

  /**
   * @brief Returns the version number of the HDF5 paths that are currently being
   * returned by this class. This is independent of the translations from HDF5 paths
   * to StorageKeys (that translation may support multiple versions).
   */
  static int getCurrentVersion() { return CURRENT_VERSION; }

  /**
   * @brief Translates the specified HDF5 'path' into the appropriate StorageKey.
   */
  static StorageKey getKeyFromString(const std::string& path, int translationVersion = CURRENT_VERSION)
  {
    std::vector<std::string> elementList;
    boost::split(elementList, path, boost::is_any_of(PATH_SEPARATOR));
    return getKeyFromList(elementList, translationVersion);
  }

  /**
   * @brief Translates the specified HDF5 'path' elements into the appropriate StorageKey.
   */
  static StorageKey getKeyFromList(const std::vector<std::string>& pathElements,
                                   int translationVersion = CURRENT_VERSION)
  {
    if (translationVersion == 1) {
      int eventId = StorageKey::INVALID_EVENTID;
      std::string detectorId = StorageKey::INVALID_DETECTORID;
      int geoLocation = StorageKey::INVALID_GEOLOCATION;

      if (pathElements.size() >= 1) {
        std::stringstream evId(pathElements[0]);
        evId >> eventId;
      }
      if (pathElements.size() >= 2) {
        std::stringstream geoLoc(pathElements[1]);
        geoLoc >> geoLocation;
      }

      return StorageKey(eventId, detectorId, geoLocation);

    } else {
      StorageKey emptyKey(StorageKey::INVALID_EVENTID, StorageKey::INVALID_DETECTORID, StorageKey::INVALID_GEOLOCATION);
      return emptyKey;
    }
  }

private:
  static const int CURRENT_VERSION = 1;
};

} // namespace ddpdemo
} // namespace dunedaq

#endif // DDPDEMO_SRC_HDF5KEYTRANSLATOR_HPP_
