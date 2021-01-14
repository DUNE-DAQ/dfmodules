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
  StorageKey data_key;
  size_t data_size;
  const void* unowned_data_start;
  std::unique_ptr<char> owned_data_start;

  // size_t trh_size;
  //const void* unowned_trigger_record_header;
  //std::unique_ptr<char> owned_trigger_record_header;


  explicit KeyedDataBlock(const StorageKey& theKey)
    : data_key(theKey)
  {}

  const void* getDataStart() const
  {
    if (owned_data_start.get() != nullptr) {
      return static_cast<const void*>(owned_data_start.get());
    } else {
      return unowned_data_start;
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


  size_t getDataSizeBytes() const { return data_size; }
  //size_t getTriggerRecordHeaderSizeBytes() const {return trh_size}
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_INCLUDE_DFMODULES_KEYEDDATABLOCK_HPP_
