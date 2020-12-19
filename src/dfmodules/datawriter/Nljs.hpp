/*
 * This file is 100% generated.  Any manual edits will likely be lost.
 *
 * This contains functions struct and other type definitions for shema in 
 * namespace dunedaq::dfmodules::datawriter to be serialized via nlohmann::json.
 */
#ifndef DUNEDAQ_DFMODULES_DATAWRITER_NLJS_HPP
#define DUNEDAQ_DFMODULES_DATAWRITER_NLJS_HPP


#include "dfmodules/datawriter/Structs.hpp"


#include <nlohmann/json.hpp>

namespace dunedaq::dfmodules::datawriter {

    using data_t = nlohmann::json;


    
    inline void to_json(data_t& j, const ConfParams& obj) {
        j["directory_path"] = obj.directory_path;
        j["mode"] = obj.mode;
    }
    
    inline void from_json(const data_t& j, ConfParams& obj) {
        if (j.contains("directory_path"))
            j.at("directory_path").get_to(obj.directory_path);    
        if (j.contains("mode"))
            j.at("mode").get_to(obj.mode);    
    }
    
    // fixme: add support for MessagePack serializers (at least)

} // namespace dunedaq::dfmodules::datawriter

#endif // DUNEDAQ_DFMODULES_DATAWRITER_NLJS_HPP