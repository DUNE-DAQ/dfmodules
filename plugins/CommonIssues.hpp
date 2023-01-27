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

#ifndef DFMODULES_PLUGINS_COMMONISSUES_HPP_
#define DFMODULES_PLUGINS_COMMONISSUES_HPP_

#include "appfwk/DAQModule.hpp"
#include "ers/Issue.hpp"

#include <string>

namespace dunedaq {

// Disable coverage collection LCOV_EXCL_START
/*ERS_DECLARE_ISSUE_BASE(dfmodules,
                       ProgressUpdate,
                       appfwk::GeneralDAQModuleIssue,
                       message,
                       ((std::string)name),
                       ((std::string)message))
*/
ERS_DECLARE_ISSUE_BASE(dfmodule,
                       InvalidQueueFatalError,
                       appfwk::GeneralDAQModuleIssue,
                       "The " << queueType << " queue was not successfully created.",
                       ((std::string)name),
                       ((std::string)queueType))
// Re-enable coverage collection LCOV_EXCL_STOP

} // namespace dunedaq

#endif // DFMODULES_PLUGINS_COMMONISSUES_HPP_