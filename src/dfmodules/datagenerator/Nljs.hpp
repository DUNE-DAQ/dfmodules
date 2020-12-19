/*
 * This file is 100% generated.  Any manual edits will likely be lost.
 *
 * This contains functions struct and other type definitions for shema in 
 * namespace dunedaq::dfmodules::datagenerator to be serialized via nlohmann::json.
 */
#ifndef DUNEDAQ_DFMODULES_DATAGENERATOR_NLJS_HPP
#define DUNEDAQ_DFMODULES_DATAGENERATOR_NLJS_HPP


#include "dfmodules/datagenerator/Structs.hpp"


#include <nlohmann/json.hpp>

namespace dunedaq::dfmodules::datagenerator {

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
        j["geo_location_count"] = obj.geo_location_count;
        j["io_size"] = obj.io_size;
        j["sleep_msec_while_running"] = obj.sleep_msec_while_running;
        j["data_store_parameters"] = obj.data_store_parameters;
    }
    
    inline void from_json(const data_t& j, Conf& obj) {
        if (j.contains("geo_location_count"))
            j.at("geo_location_count").get_to(obj.geo_location_count);    
        if (j.contains("io_size"))
            j.at("io_size").get_to(obj.io_size);    
        if (j.contains("sleep_msec_while_running"))
            j.at("sleep_msec_while_running").get_to(obj.sleep_msec_while_running);    
        if (j.contains("data_store_parameters"))
            j.at("data_store_parameters").get_to(obj.data_store_parameters);    
    }
    
    // fixme: add support for MessagePack serializers (at least)

} // namespace dunedaq::dfmodules::datagenerator

#endif // DUNEDAQ_DFMODULES_DATAGENERATOR_NLJS_HPP