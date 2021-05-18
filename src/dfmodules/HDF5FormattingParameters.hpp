/**
 * @file HDF5FormattingParameters.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_HDF5FORMATTINGPARAMETERS_HPP_
#define DFMODULES_SRC_HDF5FORMATTINGPARAMETERS_HPP_

#include "dfmodules/StorageKey.hpp"
#include <map>

namespace dunedaq {
namespace dfmodules {

class HDF5FormattingParameters
{
public:

  enum class OperationalEnvironmentType : uint16_t
  {
    kSoftwareTest = 1,
    kICEBERG = 2,
    kInvalid = 0
  };

  struct PathParameters {
    std::string system_name;
  };

  struct FilenameParameters {
    int number_of_digits_for_run_number;
  };

  static int get_current_version_number(OperationalEnvironmentType /*op_env_type*/) {
    return 1;
  }

  /**
   * @throws
   * test if accessing map element 100 fails to compile.
   */
  static std::map<StorageKey::DataRecordGroupType,PathParameters>
  get_path_parameters(OperationalEnvironmentType /*op_env_type*/, int /*param_version*/) {
    std::map<StorageKey::DataRecordGroupType,PathParameters> the_map;

    PathParameters tpc_params;
    tpc_params.system_name = "TPC";
    the_map[StorageKey::DataRecordGroupType::kTPC] = tpc_params;

    PathParameters pds_params;
    pds_params.system_name = "PDS";
    the_map[StorageKey::DataRecordGroupType::kPDS] = pds_params;

    PathParameters trigger_params;
    trigger_params.system_name = "Trigger";
    the_map[StorageKey::DataRecordGroupType::kTrigger] = trigger_params;

    PathParameters invalid_params;
    invalid_params.system_name = "Invalid";
    the_map[StorageKey::DataRecordGroupType::kInvalid] = invalid_params;

    return the_map;
  }

  /**
   * @throws
   */
  static FilenameParameters get_filename_parameters(OperationalEnvironmentType /*op_env_type*/, int /*param_version*/) {
    FilenameParameters params;
    params.number_of_digits_for_run_number = 6;
    return params;
  }


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

};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_SRC_HDF5FORMATTINGPARAMETERS_HPP_
