/*
 * This file is 100% generated.  Any manual edits will likely be lost.
 *
 * This contains functions struct and other type definitions for shema in 
 * namespace dunedaq::dfmodules::faketrigdecemu to be serialized via nlohmann::json.
 */
#ifndef DUNEDAQ_DFMODULES_FAKETRIGDECEMU_NLJS_HPP
#define DUNEDAQ_DFMODULES_FAKETRIGDECEMU_NLJS_HPP


#include "dfmodules/faketrigdecemu/Structs.hpp"


#include <nlohmann/json.hpp>

namespace dunedaq::dfmodules::faketrigdecemu {

    using data_t = nlohmann::json;


    
    inline void to_json(data_t& j, const Conf& obj) {
        j["sleep_msec_while_running"] = obj.sleep_msec_while_running;
    }
    
    inline void from_json(const data_t& j, Conf& obj) {
        if (j.contains("sleep_msec_while_running"))
            j.at("sleep_msec_while_running").get_to(obj.sleep_msec_while_running);    
    }
    
    // fixme: add support for MessagePack serializers (at least)

} // namespace dunedaq::dfmodules::faketrigdecemu

#endif // DUNEDAQ_DFMODULES_FAKETRIGDECEMU_NLJS_HPP