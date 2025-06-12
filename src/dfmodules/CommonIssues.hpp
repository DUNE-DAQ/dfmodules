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
#include "daqdataformats/Types.hpp"
#include "logging/Logging.hpp" // NOTE: if ISSUES ARE DECLARED BEFORE include logging/Logging.hpp, TLOG_DEBUG<<issue wont work.

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
 * @brief Data Request sender lookup failed
 */
ERS_DECLARE_ISSUE(dfmodules,            ///< Namespace
                  DRSenderLookupFailed, ///< Issue class name
                  "Unable to determine the Data Request message sender for SourceID [" << sid
                  << "]. No DataRequest will be sent to this data source for run/trigger/sequence number "
                  << runno << "/" << trigno << "/" << seqno << ".",
                  ((daqdataformats::SourceID)sid)            ///< Message parameters
                  ((daqdataformats::run_number_t)runno)      ///< Message parameters
                  ((daqdataformats::trigger_number_t)trigno) ///< Message parameters
                  ((daqdataformats::sequence_number_t)seqno) ///< Message parameters
)

/**
 * @brief Invalid System Type
 */
ERS_DECLARE_ISSUE(dfmodules,         ///< Namespace
                  InvalidSystemType, ///< Issue class name
                  "Unknown system type " << type,
                  ((std::string)type) ///< Message parameters
)

/**
 * @brief Data Request send failed
 */
ERS_DECLARE_ISSUE(dfmodules,           ///< Namespace
                  DRSenderSendFailed,  ///< Issue class name
                  "Failed to send Data Request message. Run: " << runno << ", trigger: " << trigno << ", trigger timestamp: "
		  << triggerts << ", sequence: " << seqno << ", destination: " << datadest << ", requested component: " << sid << ".",
		  ((daqdataformats::run_number_t)runno)          ///< Message parameters
                  ((daqdataformats::trigger_number_t)trigno)     ///< Message parameters
		  ((daqdataformats::timestamp_t)triggerts) ///< Message parameters
                  ((daqdataformats::sequence_number_t)seqno)     ///< Message parameters
		  ((std::string)datadest)                        ///< Message parameters
		  ((daqdataformats::SourceID)sid)                ///< Message parameters
)
// Re-enable coverage checking LCOV_EXCL_STOP

} // namespace dunedaq

#endif // DFMODULES_SRC_DFMODULES_COMMONISSUES_HPP_
