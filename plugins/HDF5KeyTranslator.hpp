#ifndef DFMODULES_SRC_HDF5KEYTRANSLATOR_HPP_
#define DFMODULES_SRC_HDF5KEYTRANSLATOR_HPP_
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

#include "dfmodules/StorageKey.hpp"

#include <boost/algorithm/string.hpp>

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

class HDF5KeyTranslator
{

public:
  inline static const std::string PATH_SEPARATOR = "/";

  static const int TRIGGERNUMBER_DIGITS = 6;
  static const int APANUMBER_DIGITS = 3;
  static const int LINKNUMBER_DIGITS = 2;

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

    // first, we take care of the trigger number
    std::ostringstream triggerNumberString;
    triggerNumberString << std::setw(TRIGGERNUMBER_DIGITS) << std::setfill('0') << key.getEventID();
    elementList.push_back(triggerNumberString.str());

    // Add detector type
    //elementList.push_back(key.getDetectorType);

    // next, we translate the APA number location
    std::ostringstream apaNumberString;
    apaNumberString << std::setw(APANUMBER_DIGITS) << std::setfill('0') << key.getApaNumber();
    elementList.push_back(apaNumberString.str());

    // Finally, add link number 
    std::ostringstream linkNumberString;
    linkNumberString << std::setw(LINKNUMBER_DIGITS) << std::setfill('0') << key.getLinkNumber();
    elementList.push_back(linkNumberString.str());


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
      int run_number = StorageKey::INVALID_RUNNUMBER;
      int trigger_number = StorageKey::INVALID_TRIGGERNUMBER;
      std::string detector_type = StorageKey::INVALID_DETECTORTYPE;
      int apa_number = StorageKey::INVALID_APANUMBER;
      int link_number = StorageKey::INVALID_LINKNUMBER;

      if (pathElements.size() >= 1) {
        std::stringstream runNumber(pathElements[0]);
        runNumber >> run_number;
      }
      if (pathElements.size() >= 2) {
        std::stringstream trigNumber(pathElements[1]);
        trigNumber >> trigger_number;
      }

      if (pathElements.size() >= 3) {
        std::stringstream detectorType(pathElements[2]);
        detectorType >> detector_type;
      }

      if (pathElements.size() >= 4) {
        std::stringstream apaNumber(pathElements[3]);
        apaNumber >> apa_number;
      }

      if (pathElements.size() >= 5) {
        std::stringstream linkNumber(pathElements[4]);
        linkNumber >> link_number;
      }


      return StorageKey(run_number, trigger_number, detector_type, apa_number, link_number);



    } else {
      StorageKey emptyKey(StorageKey::INVALID_RUNNUMBER, StorageKey::INVALID_TRIGGERNUMBER, StorageKey::INVALID_DETECTORTYPE, StorageKey::INVALID_APANUMBER, StorageKey::INVALID_LINKNUMBER);
      return emptyKey;
    }
  }

private:
  static const int CURRENT_VERSION = 1;
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_HDF5KEYTRANSLATOR_HPP_
