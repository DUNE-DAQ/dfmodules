/**
 * @file StorageKey.hpp Collection of parameters that identify a block of data
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_INCLUDE_DFMODULES_STORAGEKEY_HPP_
#define DFMODULES_INCLUDE_DFMODULES_STORAGEKEY_HPP_

#include <limits>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief The StorageKey class defines the collection of parameters that
 * identify a given block of data.
 */
class StorageKey
{
public:
  static constexpr int s_invalid_run_number = std::numeric_limits<int>::max();
  static constexpr int s_invalid_trigger_number = std::numeric_limits<int>::max();
  static constexpr int s_invalid_region_number = std::numeric_limits<int>::max();
  static constexpr int s_invalid_element_number = std::numeric_limits<int>::max();

  /**
   * @brief The group that should be used within the data record.
   */
  enum DataRecordGroupType
  {
    kTriggerRecordHeader = 1,
    kTPC = 2,
    kPDS = 3,
    kTrigger = 4,
    kInvalid = 0
  };

  StorageKey(int run_number, int trigger_number, DataRecordGroupType group_type, int region_number, int element_number) noexcept
    : m_run_number(run_number)
    , m_trigger_number(trigger_number)
    , m_group_type(group_type)
    , m_region_number(region_number)
    , m_element_number(element_number)

  {}
  ~StorageKey() {} // NOLINT

  int get_run_number() const;
  int get_trigger_number() const;
  DataRecordGroupType get_group_type() const;
  int get_region_number() const;
  int get_element_number() const;

private:
  int m_run_number;
  int m_trigger_number;
  DataRecordGroupType m_group_type;
  int m_region_number;
  int m_element_number;
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_INCLUDE_DFMODULES_STORAGEKEY_HPP_
