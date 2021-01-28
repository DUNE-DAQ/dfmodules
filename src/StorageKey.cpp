/**
 * @file StorageKey.hpp
 *
 * StorageKey class used to identify a given block of data
 *
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/StorageKey.hpp"

#include "ers/ers.h"

#include <string>

namespace dunedaq {
namespace dfmodules {

int
StorageKey::get_run_number() const
{
  return m_key.m_run_number;
}

int
StorageKey::get_trigger_number() const
{
  return m_key.m_trigger_number;
}

std::string
StorageKey::get_detector_type() const
{
  return m_key.m_detector_type;
}

int
StorageKey::get_apa_number() const
{
  return m_key.m_apa_number;
}

int
StorageKey::get_link_number() const
{
  return m_key.m_link_number;
}

} // namespace dfmodules
} // namespace dunedaq
