/*
 * This file is 100% generated.  Any manual edits will likely be lost.
 *
 * This contains functions struct and other type definitions for shema in 
 * namespace dunedaq::dfmodules::datatransfermodule to be serialized via nlohmann::json.
 */
#ifndef DUNEDAQ_DFMODULES_DATATRANSFERMODULE_NLJS_HPP
#define DUNEDAQ_DFMODULES_DATATRANSFERMODULE_NLJS_HPP


#include "dfmodules/datatransfermodule/Structs.hpp"


#include <nlohmann/json.hpp>

namespace dunedaq::dfmodules::datatransfermodule {

    using data_t = nlohmann::json;


    
    inline void to_json(data_t& j, const DataStore& obj) {
        j["type"] = obj.type;
        j["name"] = obj.name;
        j["directory_path"] = obj.directory_path;
        j["filename_prefix"] = obj.filename_prefix;
        j["mode"] = obj.mode;
    }
    
    inline void from_json(const data_t& j, DataStore& obj) {
        if (j.contains("type"))
            j.at("type").get_to(obj.type);    
        if (j.contains("name"))
            j.at("name").get_to(obj.name);    
        if (j.contains("directory_path"))
            j.at("directory_path").get_to(obj.directory_path);    
        if (j.contains("filename_prefix"))
            j.at("filename_prefix").get_to(obj.filename_prefix);    
        if (j.contains("mode"))
            j.at("mode").get_to(obj.mode);    
    }
    
    inline void to_json(data_t& j, const Conf& obj) {
        j["sleep_msec_while_running"] = obj.sleep_msec_while_running;
        j["input_data_store_parameters"] = obj.input_data_store_parameters;
        j["output_data_store_parameters"] = obj.output_data_store_parameters;
    }
    
    inline void from_json(const data_t& j, Conf& obj) {
        if (j.contains("sleep_msec_while_running"))
            j.at("sleep_msec_while_running").get_to(obj.sleep_msec_while_running);    
        if (j.contains("input_data_store_parameters"))
            j.at("input_data_store_parameters").get_to(obj.input_data_store_parameters);    
        if (j.contains("output_data_store_parameters"))
            j.at("output_data_store_parameters").get_to(obj.output_data_store_parameters);    
    }
    
    // fixme: add support for MessagePack serializers (at least)

} // namespace dunedaq::dfmodules::datatransfermodule

#endif // DUNEDAQ_DFMODULES_DATATRANSFERMODULE_NLJS_HPP