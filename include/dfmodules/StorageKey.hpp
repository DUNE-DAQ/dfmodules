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

#include <limits>
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
  Key(int run_number, int trigger_number, std::string detector_type, int apa_number, int link_number) noexcept
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
  static constexpr int s_invalid_run_number = std::numeric_limits<int>::max();
  static constexpr int s_invalid_trigger_number = std::numeric_limits<int>::max();
  inline static const std::string s_invalid_detector_type = "Invalid";
  static constexpr int s_invalid_apa_number = std::numeric_limits<int>::max();
  static constexpr int s_invalid_link_number = std::numeric_limits<int>::max();

  StorageKey(int run_number, int trigger_number, std::string detector_type, int apa_number, int link_number) noexcept
    : m_key(run_number, trigger_number, detector_type, apa_number, link_number)
  {}
  ~StorageKey() {} // NOLINT

  int get_run_number() const;
  int get_trigger_number() const;
  std::string get_detector_type() const;
  int get_apa_number() const;
  int get_link_number() const;

private:
  Key m_key;
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_INCLUDE_DFMODULES_STORAGEKEY_HPP_
