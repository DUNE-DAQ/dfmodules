/**
 * @file HDF5FormattingParameters.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DFMODULES_HDF5FORMATTINGPARAMETERS_HPP_
#define DFMODULES_SRC_DFMODULES_HDF5FORMATTINGPARAMETERS_HPP_

#include "dfmodules/StorageKey.hpp"

#include <map>
#include <string>

namespace dunedaq {
namespace dfmodules {

class HDF5FormattingParameters
{

public:
  enum class OperationalEnvironmentType : uint16_t // NOLINT(build/unsigned)
  {
    kSoftwareTest = 1,
    kICEBERG = 2,
    kInvalid = 0
  };

  struct DataRecordParameters
  {
    std::string trigger_record_name_prefix;
    int digits_for_trigger_number;
  };

  struct PathParameters
  {
    std::string group_name_within_data_record;
    std::string region_name_prefix;
    int digits_for_region_number;
    std::string element_name_prefix;
    int digits_for_element_number;
  };

  struct FilenameParameters
  {
    int number_of_digits_for_run_number;
  };

  static int get_current_version_number(/*OperationalEnvironmentType op_env_type*/) { return 1; }

  static DataRecordParameters get_data_record_parameters(/*OperationalEnvironmentType op_env_type,*/
                                                         int /*param_version*/)
  {
    DataRecordParameters dr_params;
    dr_params.trigger_record_name_prefix = "TriggerRecord";
    dr_params.digits_for_trigger_number = 5;
    return dr_params;
  }

  static std::map<StorageKey::DataRecordGroupType, PathParameters>
  get_path_parameters(/*OperationalEnvironmentType op_env_type,*/
                      int /*param_version*/)
  {
    std::map<StorageKey::DataRecordGroupType, PathParameters> the_map;

    PathParameters tpc_params;
    tpc_params.group_name_within_data_record = "TPC";
    tpc_params.region_name_prefix = "APA";
    tpc_params.digits_for_region_number = 3;
    tpc_params.element_name_prefix = "Link";
    tpc_params.digits_for_element_number = 2;
    the_map[StorageKey::DataRecordGroupType::kTPC] = tpc_params;

    PathParameters pds_params;
    pds_params.group_name_within_data_record = "PDS";
    pds_params.region_name_prefix = "Region";
    pds_params.digits_for_region_number = 3;
    pds_params.element_name_prefix = "Element";
    pds_params.digits_for_element_number = 2;
    the_map[StorageKey::DataRecordGroupType::kPDS] = pds_params;

    PathParameters trigger_params;
    trigger_params.group_name_within_data_record = "Trigger";
    trigger_params.region_name_prefix = "Region";
    trigger_params.digits_for_region_number = 3;
    trigger_params.element_name_prefix = "Element";
    trigger_params.digits_for_element_number = 2;
    the_map[StorageKey::DataRecordGroupType::kTrigger] = trigger_params;

    PathParameters invalid_params;
    invalid_params.group_name_within_data_record = "Invalid";
    invalid_params.region_name_prefix = "Region";
    invalid_params.digits_for_region_number = 3;
    invalid_params.element_name_prefix = "Element";
    invalid_params.digits_for_element_number = 2;
    the_map[StorageKey::DataRecordGroupType::kInvalid] = invalid_params;

    return the_map;
  }

  static FilenameParameters get_filename_parameters(/*OperationalEnvironmentType op_env_type,*/
                                                    int /*param_version*/)
  {
    FilenameParameters params;
    params.number_of_digits_for_run_number = 6;
    return params;
  }

#if 0
  static std::string op_env_type_to_string(OperationalEnvironmentType type)
  {
    switch (type) {
      case OperationalEnvironmentType::kSoftwareTest:
        return "swtest";
      case OperationalEnvironmentType::kICEBERG:
        return "iceberg";
      case OperationalEnvironmentType::kInvalid:
        return "invalid";
    }
    return "invalid";
  }

  static OperationalEnvironmentType string_to_op_env_type(std::string typestring)
  {
    if (typestring.find("swtest") == 0)
      return OperationalEnvironmentType::kSoftwareTest;
    if (typestring.find("iceberg") == 0)
      return OperationalEnvironmentType::kICEBERG;
    return OperationalEnvironmentType::kInvalid;
  }
#endif
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_DFMODULES_HDF5FORMATTINGPARAMETERS_HPP_
