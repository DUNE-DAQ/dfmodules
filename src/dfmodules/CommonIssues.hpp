/**
 * @file CommonIssues.hpp
 *
 * This file contains the definitions of ERS Issues that are common
 * to two or more of the DAQModules in this package.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DFMODULES_COMMONISSUES_HPP_
#define DFMODULES_SRC_DFMODULES_COMMONISSUES_HPP_

#include "appfwk/DAQModule.hpp"
#include "daqdataformats/SourceID.hpp"

#include <string>

namespace dunedaq {

// Disable coverage checking LCOV_EXCL_START

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       ProgressUpdate,
                       appfwk::GeneralDAQModuleIssue,
                       message,
                       ((std::string)name),
                       ((std::string)message))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidQueueFatalError,
                       appfwk::GeneralDAQModuleIssue,
                       "The " << queue_type << " queue was not successfully created.",
                       ((std::string)name),
                       ((std::string)queue_type))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       InvalidHDF5Group,
                       appfwk::GeneralDAQModuleIssue,
                       "The HDF5 Group associated with name \"" << group_name << "\" is invalid.",
                       ((std::string)name),
                       ((std::string)group_name))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       UnableToConfigure,
                       appfwk::GeneralDAQModuleIssue,
                       "Unable to successfully configure.",
                       ((std::string)name),
                       ERS_EMPTY)

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       UnableToStart,
                       appfwk::GeneralDAQModuleIssue,
                       "Unable to start run " << run_number << ".",
                       ((std::string)name),
                       ((size_t)run_number))

ERS_DECLARE_ISSUE_BASE(dfmodules,
                       ProblemDuringStop,
                       appfwk::GeneralDAQModuleIssue,
                       "A problem was enountered during the stopping of run " << run_number << ".",
                       ((std::string)name),
                       ((size_t)run_number))

/**
 * @brief Unknown SourceID
 */
ERS_DECLARE_ISSUE(dfmodules,    ///< Namespace
                  UnknownSourceID, ///< Issue class name
                  "Unknown SourceID: " << source_id,
                  ((daqdataformats::SourceID)source_id) ///< Message parameters
)

/**
 * @brief Invalid System Type
 */
ERS_DECLARE_ISSUE(dfmodules,         ///< Namespace
                  InvalidSystemType, ///< Issue class name
                  "Unknown system type " << type,
                  ((std::string)type) ///< Message parameters
)
// Re-enable coverage checking LCOV_EXCL_STOP

} // namespace dunedaq

#endif // DFMODULES_SRC_DFMODULES_COMMONISSUES_HPP_
