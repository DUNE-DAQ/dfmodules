/**
 * @file TPBundleHandler.cpp TPBundleHandler Class Implementation
 *
 * The TPBundleHandler class takes care of assembling and repacking TriggerPrimitives
 * for storage on disk as part of a TP stream.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/TPBundleHandler.hpp"

#include "logging/Logging.hpp"

#include <memory>
#include <string>
#include <utility>

namespace dunedaq {
namespace dfmodules {

void
TimeSliceAccumulator::add_tpset(trigger::TPSet&& tpset)
{
  // create an entry in the top-level map for this geoid, if needed
  {
    auto lk = std::lock_guard<std::mutex>(m_bundle_map_mutex);
    if (m_tpbundles_by_geoid_and_start_time.count(tpset.origin) == 0) {
      tpbundles_by_start_time_t empty_bundle_map;
      m_tpbundles_by_geoid_and_start_time[tpset.origin] = empty_bundle_map;
    }
  }

  // Create a TPBundle from the TPSet.
  // NOTE that the resulting TPBundle might have fewer TriggerPrimitives in it
  // than the input TPSet
  // because we take this opportunity to ignore TPs that are outside the
  // window of interest for this accumulator.
  TPBundle tp_bundle;
  tp_bundle.geoid = tpset.origin;
  tp_bundle.tplist.reserve(tpset.objects.size());
  tp_bundle.first_time = daqdataformats::TypeDefaults::s_invalid_timestamp;
  tp_bundle.last_time = daqdataformats::TypeDefaults::s_invalid_timestamp;
  bool first = true;
  for (auto& oldtp : tpset.objects) {
    if (oldtp.time_start < m_begin_time || oldtp.time_start >= m_end_time) {
      continue;
    }

    if (first) {
      tp_bundle.first_time = oldtp.time_start;
      first = false;
    }
    tp_bundle.last_time = oldtp.time_start;

    detdataformats::trigger::TriggerPrimitive newtp;
    newtp.time_start = oldtp.time_start;
    newtp.time_peak = oldtp.time_peak;
    newtp.time_over_threshold = oldtp.time_over_threshold;
    newtp.channel = oldtp.channel;
    newtp.adc_integral = oldtp.adc_integral;
    newtp.adc_peak = oldtp.adc_peak;
    newtp.detid = oldtp.detid;
    switch (oldtp.type) {
      case triggeralgs::TriggerPrimitive::Type::kTPC:
        newtp.type = detdataformats::trigger::TriggerPrimitive::Type::kTPC;
        break;
      case triggeralgs::TriggerPrimitive::Type::kPDS:
        newtp.type = detdataformats::trigger::TriggerPrimitive::Type::kPDS;
        break;
      default:
        newtp.type = detdataformats::trigger::TriggerPrimitive::Type::kUnknown;
    }
    switch (oldtp.algorithm) {
      case triggeralgs::TriggerPrimitive::Algorithm::kTPCDefault:
        newtp.algorithm = detdataformats::trigger::TriggerPrimitive::Algorithm::kTPCDefault;
        break;
      default:
        newtp.algorithm = detdataformats::trigger::TriggerPrimitive::Algorithm::kUnknown;
    }
    newtp.version = oldtp.version;
    newtp.flag = oldtp.flag;
    tp_bundle.tplist.push_back(std::move(newtp));
  }

  // store the bundle in the map
  if (tp_bundle.tplist.size() == 0) {
    if (tpset.end_time == m_begin_time) {
      // the end of the TPSet just missed the start of our window, so not a big deal
      TLOG() << "Note: no TPs were used from a TPSet with start_time=" << tpset.start_time
             << ", end_time=" << tpset.end_time << ", tpbundle begin and end times:" << m_begin_time << ", "
             << m_end_time;
    } else {
      // woah, something unexpected happened
      TLOG() << "WARNING: no TPs were used from a TPSet with start_time=" << tpset.start_time
             << ", end_time=" << tpset.end_time << ", tpbundle begin and end times:" << m_begin_time << ", "
             << m_end_time;
    }
  } else {
    auto lk = std::lock_guard<std::mutex>(m_bundle_map_mutex);
    daqdataformats::GeoID geoid = tp_bundle.geoid;
    daqdataformats::timestamp_t start_time = tp_bundle.first_time;
    m_tpbundles_by_geoid_and_start_time[geoid].emplace(start_time, std::move(tp_bundle));
    m_update_time = std::chrono::steady_clock::now();
  }
}

std::unique_ptr<daqdataformats::TriggerRecord> TimeSliceAccumulator::get_timeslice()
{
  auto lk = std::lock_guard<std::mutex>(m_bundle_map_mutex);

  std::vector<daqdataformats::ComponentRequest> list_of_components;
  std::vector<std::unique_ptr<daqdataformats::Fragment>> list_of_fragments;

  // loop over all GeoIDs present in this accumulator
  for (auto& [geoid, bundle_map] : m_tpbundles_by_geoid_and_start_time) {
    daqdataformats::timestamp_t first_time = daqdataformats::TypeDefaults::s_invalid_timestamp;
    daqdataformats::timestamp_t last_time = daqdataformats::TypeDefaults::s_invalid_timestamp;

    // build up the list of pieces that we will use to contruct the Fragment
    std::vector<std::pair<void*, size_t>> list_of_pieces;
    bool first = true;
    for (auto& [start_time, tp_bundle] : bundle_map) {
      if (tp_bundle.tplist.size() == 0) {
        // this shouldn't happen, but we check anyway so that we don't throw an uncaught exception later
        continue;
      }
      if (first) {
        first_time = tp_bundle.first_time;
        first = false;
      }
      last_time = tp_bundle.last_time;
      list_of_pieces.push_back(std::make_pair<void*, size_t>(
        &tp_bundle.tplist[0], tp_bundle.tplist.size() * sizeof(detdataformats::trigger::TriggerPrimitive)));
    }
    TLOG() << "number of pieces is " << list_of_pieces.size();
    std::unique_ptr<daqdataformats::Fragment> frag(new daqdataformats::Fragment(list_of_pieces));

    frag->set_run_number(m_run_number);
    frag->set_window_begin(first_time);
    frag->set_window_end(last_time);
    frag->set_element_id(geoid);
    frag->set_type(daqdataformats::FragmentType::kTriggerPrimitives);

    size_t frag_payload_size = frag->get_size() - sizeof(dunedaq::daqdataformats::FragmentHeader);
    TLOG() << "In get_timeslice, GeoID is " << geoid << ", number of pieces is " << list_of_pieces.size()
           << ", size of Fragment payload is " << frag_payload_size;

    list_of_fragments.push_back(std::move(frag));
    daqdataformats::ComponentRequest creq;
    creq.component = geoid;
    creq.window_begin = m_begin_time;
    creq.window_end = m_end_time;
    list_of_components.push_back(creq);
  }

  std::unique_ptr<daqdataformats::TriggerRecord> trig_rec(new daqdataformats::TriggerRecord(list_of_components));
  trig_rec->set_fragments(std::move(list_of_fragments));
  return trig_rec;
}

void
TPBundleHandler::add_tpset(trigger::TPSet&& tpset)
{
  // if the tpset seems to span multiple timeslices, then we add it to all
  // of the relevant accumulators. This runs the risk of adding a tpset
  // to an accumululator that won't find any TPs within the
  // accumulator window (because of edge effects), but we want to be
  // cautious here (and we'll protect against the absence of tpsets later).
  // Of course, adding the tpset to multiple accumulators requires copies...
  size_t tsidx_from_begin_time = tpset.start_time / m_slice_interval;
  size_t tsidx_from_end_time = tpset.end_time / m_slice_interval;

  // add the TPSet to any 'extra' accumulators
  for (size_t tsidx = (tsidx_from_begin_time + 1); tsidx <= tsidx_from_end_time; ++tsidx) {
    {
      auto lk = std::lock_guard<std::mutex>(m_accumulator_map_mutex);
      if (m_timeslice_accumulators.count(tsidx) == 0) {
        TimeSliceAccumulator accum(tsidx * m_slice_interval, (tsidx + 1) * m_slice_interval, m_run_number);
        m_timeslice_accumulators[tsidx] = accum;
      }
    }
    trigger::TPSet tpset_copy = tpset;
    m_timeslice_accumulators[tsidx].add_tpset(std::move(tpset_copy));
  }

  // add the TPSet to the accumulator associated with the begin time
  {
    auto lk = std::lock_guard<std::mutex>(m_accumulator_map_mutex);
    if (m_timeslice_accumulators.count(tsidx_from_begin_time) == 0) {
      TimeSliceAccumulator accum(
        tsidx_from_begin_time * m_slice_interval, (tsidx_from_begin_time + 1) * m_slice_interval, m_run_number);
      m_timeslice_accumulators[tsidx_from_begin_time] = accum;
    }
  }
  m_timeslice_accumulators[tsidx_from_begin_time].add_tpset(std::move(tpset));
}

std::vector<std::unique_ptr<daqdataformats::TriggerRecord>> 
TPBundleHandler::get_properly_aged_timeslices()
{
  std::vector<std::unique_ptr<daqdataformats::TriggerRecord>> list_of_timeslices;
  std::vector<daqdataformats::timestamp_t> elements_to_be_removed;

  auto now = std::chrono::steady_clock::now();
  for (auto& [tsidx, accum] : m_timeslice_accumulators) {
    if ((now - accum.get_update_time()) >= m_cooling_off_time) {
      TLOG() << "Fetching timeslice for " << tsidx;
      list_of_timeslices.push_back(accum.get_timeslice());
      elements_to_be_removed.push_back(tsidx);
    }
  }

  auto lk = std::lock_guard<std::mutex>(m_accumulator_map_mutex);
  for (auto& tsidx : elements_to_be_removed) {
    m_timeslice_accumulators.erase(tsidx);
  }

  return list_of_timeslices;
}

} // namespace dfmodules
} // namespace dunedaq
