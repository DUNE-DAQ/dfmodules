/**
 * @file DFOConsensusModule.hpp
 *
 * Thin compatibility wrapper for the legacy DFOConsensusModule plugin.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_DFOCONSENSUSMODULE_HPP_
#define DFMODULES_PLUGINS_DFOCONSENSUSMODULE_HPP_

#include "DFOModule.hpp"

namespace dunedaq::dfmodules {

class DFOConsensusModule : public DFOModule
{
public:
  explicit DFOConsensusModule(const std::string& name);

  DFOConsensusModule(const DFOConsensusModule&) = delete;
  DFOConsensusModule& operator=(const DFOConsensusModule&) = delete;
  DFOConsensusModule(DFOConsensusModule&&) = delete;
  DFOConsensusModule& operator=(DFOConsensusModule&&) = delete;
};

} // namespace dunedaq::dfmodules

#endif // DFMODULES_PLUGINS_DFOCONSENSUSMODULE_HPP_
