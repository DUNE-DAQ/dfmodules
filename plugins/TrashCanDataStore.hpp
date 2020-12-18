#ifndef DDPDEMO_SRC_TRASHCANDATASTORE_HPP_
#define DDPDEMO_SRC_TRASHCANDATASTORE_HPP_

/**
 * @file TrashCanDataStore.hpp
 *
 * An implementation of the DataStore interface that simply throws
 * data away instead of storing it.  Obviously, this class is just
 * for demonstration and testing purposes.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "ddpdemo/DataStore.hpp"

#include <ers/Issue.h>
#include <TRACE/trace.h>

#include <iomanip>
#include <string>
#include <vector>
#include <sstream>

namespace dunedaq {
namespace ddpdemo {

class TrashCanDataStore : public DataStore
{
public:
  explicit TrashCanDataStore( const nlohmann::json & conf ) 
    : DataStore( conf["name"].get<std::string>() )
  { ; }

  virtual void write(const KeyedDataBlock& dataBlock) override 
  {
    const void* dataPtr = dataBlock.getDataStart();

    const uint8_t * interpreted_data_ptr = reinterpret_cast<const uint8_t*>( dataPtr ) ;  // NOLINT 

    std::stringstream msg ;
    msg << "Throwing away the data from event ID "
	<< dataBlock.data_key.getEventID() << ", which has size of " << dataBlock.data_size
	<< " bytes, and the following data "
	<< "in the first few bytes: 0x" << std::hex << std::setfill('0') << std::setw(2)
	<< interpreted_data_ptr[0] << " 0x" << interpreted_data_ptr[1] << " 0x"
	<< interpreted_data_ptr[2] << " 0x" << interpreted_data_ptr[3] << std::dec ;

    TLOG(TLVL_INFO) << msg.str() ;
  }

  virtual std::vector<StorageKey> getAllExistingKeys() const override 
  {
    std::vector<StorageKey> emptyList;
    return emptyList;
  }

  virtual void setup(const size_t) override { ; } 

  virtual KeyedDataBlock read(const StorageKey& key) override { 
    return  KeyedDataBlock (key);
  }

private:
  // Delete the copy and move operations
  TrashCanDataStore(const TrashCanDataStore&) = delete;
  TrashCanDataStore& operator=(const TrashCanDataStore&) = delete;
  TrashCanDataStore(TrashCanDataStore&&) = delete;
  TrashCanDataStore& operator=(TrashCanDataStore&&) = delete;
};

} // namespace ddpdemo
} // namespace dunedaq

#endif // DDPDEMO_SRC_TRASHCANDATASTORE_HPP_

// Local Variables:
// c-basic-offset: 2
// End:
