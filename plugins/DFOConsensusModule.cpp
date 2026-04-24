/**
 * @file DFOConsensusModule.cpp
 *
 * Thin compatibility wrapper for the legacy DFOConsensusModule plugin.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "DFOConsensusModule.hpp"

namespace dunedaq::dfmodules {

DFOConsensusModule::DFOConsensusModule(const std::string& name)
  : DFOModule(name)
{
  set_force_consensus_mode(true);
}

} // namespace dunedaq::dfmodules

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::DFOConsensusModule)
