/**
 * @file TPBundleHandler.cpp TPBundleHandler Class Implementation
 *
 * The TPBundleHandler class takes care of assembling and repacking TriggerPrimitives
 * for storage on disk as part of a TP stream.
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
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
  // double-check that this TPSet belongs in this accumulator
  // (reverse of tpset.start_time < m_end_time || tpset.end_time > m_begin_time)
  if (tpset.end_time <= m_begin_time || tpset.start_time >= m_end_time) {
    if (tpset.end_time == m_begin_time) {
      // the end of the TPSet just missed the start of our window, so not a big deal
      TLOG() << "Note: no TPs were used from a TPSet with start_time=" << tpset.start_time
             << ", end_time=" << tpset.end_time << ", TSAccumulator begin and end times:" << m_begin_time << ", "
             << m_end_time;
    } else {
      // woah, something unexpected happened
      TLOG() << "WARNING: no TPs were used from a TPSet with start_time=" << tpset.start_time
             << ", end_time=" << tpset.end_time << ", TSAccumulator begin and end times:" << m_begin_time << ", "
             << m_end_time;
    }
    return;
  }

  // create an entry in the top-level map for the geoid in this TPSet, if needed
  auto lk = std::lock_guard<std::mutex>(m_bundle_map_mutex);
  if (m_tpbundles_by_geoid_and_start_time.count(tpset.origin) == 0) {
    tpbundles_by_start_time_t empty_bundle_map;
    m_tpbundles_by_geoid_and_start_time[tpset.origin] = empty_bundle_map;
  }

  // store the TPSet in the map
  m_tpbundles_by_geoid_and_start_time[tpset.origin].emplace(tpset.start_time, std::move(tpset));
  m_update_time = std::chrono::steady_clock::now();
}

std::unique_ptr<daqdataformats::TimeSlice>
TimeSliceAccumulator::get_timeslice()
{
  auto lk = std::lock_guard<std::mutex>(m_bundle_map_mutex);

  std::vector<std::unique_ptr<daqdataformats::Fragment>> list_of_fragments;

  // loop over all GeoIDs present in this accumulator
  for (auto& [geoid, bundle_map] : m_tpbundles_by_geoid_and_start_time) {
    daqdataformats::timestamp_t first_time = daqdataformats::TypeDefaults::s_invalid_timestamp;
    daqdataformats::timestamp_t last_time = daqdataformats::TypeDefaults::s_invalid_timestamp;

    // build up the list of pieces that we will use to contruct the Fragment
    std::vector<std::pair<void*, size_t>> list_of_pieces;
    bool first = true;
    for (auto& [start_time, tpset] : bundle_map) {
      if (first) {
        first_time = tpset.start_time;
        first = false;
      }
      last_time = tpset.end_time;
      list_of_pieces.push_back(std::make_pair<void*, size_t>(
        &tpset.objects[0], tpset.objects.size() * sizeof(detdataformats::trigger::TriggerPrimitive)));
    }
    std::unique_ptr<daqdataformats::Fragment> frag(new daqdataformats::Fragment(list_of_pieces));

    frag->set_run_number(m_run_number);
    frag->set_window_begin(first_time);
    frag->set_window_end(last_time);
    frag->set_element_id(geoid);
    frag->set_type(daqdataformats::FragmentType::kTriggerPrimitives);

    size_t frag_payload_size = frag->get_size() - sizeof(dunedaq::daqdataformats::FragmentHeader);
    TLOG() << "In get_timeslice, GeoID is " << geoid << ", number of pieces is " << list_of_pieces.size()
           << ", size of Fragment payload is " << frag_payload_size << ", size of TP is "
           << sizeof(detdataformats::trigger::TriggerPrimitive);

    list_of_fragments.push_back(std::move(frag));
  }

  std::unique_ptr<daqdataformats::TimeSlice> time_slice(new daqdataformats::TimeSlice(m_slice_number, m_run_number));
  time_slice->set_fragments(std::move(list_of_fragments));
  return time_slice;
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
  if (m_slice_index_offset == 0) {
    m_slice_index_offset = tsidx_from_begin_time - 1;
  }

  // add the TPSet to any 'extra' accumulators
  for (size_t tsidx = (tsidx_from_begin_time + 1); tsidx <= tsidx_from_end_time; ++tsidx) {
    {
      auto lk = std::lock_guard<std::mutex>(m_accumulator_map_mutex);
      if (m_timeslice_accumulators.count(tsidx) == 0) {
        TimeSliceAccumulator accum(
          tsidx * m_slice_interval, (tsidx + 1) * m_slice_interval, tsidx - m_slice_index_offset, m_run_number);
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
      TimeSliceAccumulator accum(tsidx_from_begin_time * m_slice_interval,
                                 (tsidx_from_begin_time + 1) * m_slice_interval,
                                 tsidx_from_begin_time - m_slice_index_offset,
                                 m_run_number);
      m_timeslice_accumulators[tsidx_from_begin_time] = accum;
    }
  }
  m_timeslice_accumulators[tsidx_from_begin_time].add_tpset(std::move(tpset));
}

std::vector<std::unique_ptr<daqdataformats::TimeSlice>>
TPBundleHandler::get_properly_aged_timeslices()
{
  std::vector<std::unique_ptr<daqdataformats::TimeSlice>> list_of_timeslices;
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
