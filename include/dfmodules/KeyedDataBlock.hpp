/**
 * @file KeyedDataBlock.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_INCLUDE_DFMODULES_KEYEDDATABLOCK_HPP_
#define DFMODULES_INCLUDE_DFMODULES_KEYEDDATABLOCK_HPP_

#include "dfmodules/StorageKey.hpp"

#include <cstdint>
#include <memory>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief comment
 */
struct KeyedDataBlock
{
public:
  // These data members will be made private, at some point in time.
  StorageKey m_data_key;
  size_t m_data_size;
  const void* m_unowned_data_start;
  std::unique_ptr<char> m_owned_data_start;

  // size_t trh_size;
  // const void* unowned_trigger_record_header;
  // std::unique_ptr<char> owned_trigger_record_header;

  explicit KeyedDataBlock(const StorageKey& theKey)
    : m_data_key(theKey)
  {}

  const void* get_data_start() const
  {
    if (m_owned_data_start.get() != nullptr) {
      return static_cast<const void*>(m_owned_data_start.get());
    } else {
      return m_unowned_data_start;
    }
  }

  //
  //  const void* getTriggerRecordHeader() const
  //  {
  //    if (owned_trigger_record_header.get() != nullptr) {
  //      return static_cast<const void*>(owned_trigger_record_header.get());
  //    } else {
  //      return unowned_trigger_record_header;
  //    }
  //  }

  size_t get_data_size_bytes() const { return m_data_size; }
  // size_t getTriggerRecordHeaderSizeBytes() const {return trh_size}
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_INCLUDE_DFMODULES_KEYEDDATABLOCK_HPP_
