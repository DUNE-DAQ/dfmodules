/*
 * This file is 100% generated.  Any manual edits will likely be lost.
 *
 * This contains struct and other type definitions for shema in 
 * namespace dunedaq::dfmodules::faketrigdecemu.
 */
#ifndef DUNEDAQ_DFMODULES_FAKETRIGDECEMU_STRUCTS_HPP
#define DUNEDAQ_DFMODULES_FAKETRIGDECEMU_STRUCTS_HPP

#include <cstdint>


namespace dunedaq::dfmodules::faketrigdecemu {

    // @brief A count of not too many things
    using Count = int32_t;

    // @brief FakeTrigDecEmu configuration
    struct Conf {

        // @brief Millisecs to sleep between generating data
        Count sleep_msec_while_running = 1000;
    };

} // namespace dunedaq::dfmodules::faketrigdecemu

#endif // DUNEDAQ_DFMODULES_FAKETRIGDECEMU_STRUCTS_HPP