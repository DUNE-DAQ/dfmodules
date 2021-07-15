/**
 * @file TriggerRecordBuilder.cpp TriggerRecordBuilder class implementation
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "TriggerRecordBuilder.hpp"
#include "dfmodules/CommonIssues.hpp"

#include "appfwk/DAQModuleHelper.hpp"
#include "appfwk/app/Nljs.hpp"
#include "dfmodules/triggerrecordbuilder/Nljs.hpp"
#include "dfmodules/triggerrecordbuilder/Structs.hpp"
#include "logging/Logging.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <limits>

/**
 * @brief TRACE debug levels used in this source file
 */
enum {
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_WORK_STEPS = 10,
  TLVL_BOOKKEEPING = 15
};

namespace dunedaq {
namespace dfmodules {

using dataformats::TriggerRecordErrorBits;

TriggerRecordBuilder::TriggerRecordBuilder(const std::string &name)
    : dunedaq::appfwk::DAQModule(name),
      m_thread(std::bind(&TriggerRecordBuilder::do_work, this,
                         std::placeholders::_1)),
      m_queue_timeout(100) {

  register_command("conf", &TriggerRecordBuilder::do_conf);
  register_command("start", &TriggerRecordBuilder::do_start);
  register_command("stop", &TriggerRecordBuilder::do_stop);
}

void TriggerRecordBuilder::init(const data_t &init_data) {

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)
      << get_name() << ": Entering init() method";

  //--------------------------------
  // Get single queues
  //---------------------------------

  auto qi = appfwk::queue_index(init_data, {"trigger_decision_input_queue",
                                            "trigger_record_output_queue"});
  // data request input queue
  try {
    auto temp_info = qi["trigger_decision_input_queue"];
    std::string temp_name = temp_info.inst;
    trigger_decision_source_t test(temp_name);
    m_trigger_decision_source_name = temp_name;
  } catch (const ers::Issue &excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(),
                                 "trigger_decision_input_queue", excpt);
  }

  // trigger record output
  try {
    auto temp_info = qi["trigger_record_output_queue"];
    std::string temp_name = temp_info.inst;
    trigger_record_sink_t test(temp_name);
    m_trigger_record_sink_name = temp_name;
  } catch (const ers::Issue &excpt) {
    throw InvalidQueueFatalError(ERS_HERE, get_name(),
                                 "trigger_record_output_queue", excpt);
  }

  //----------------------
  // Get dynamic queues
  //----------------------

  // set names for the fragment queue(s)
  auto ini = init_data.get<appfwk::app::ModInit>();

  // get the names for the fragment queues
  for (const auto &qitem : ini.qinfos) {
    if (qitem.name.rfind("data_fragment_") == 0) {
      try {
        std::string temp_name = qitem.inst;
        fragment_source_t test(temp_name);
        m_fragment_source_names.push_back(temp_name);
      } catch (const ers::Issue &excpt) {
        throw InvalidQueueFatalError(ERS_HERE, get_name(), qitem.name, excpt);
      }
    }
  }

  // Test for valid output data request queues
  for (const auto &qitem : ini.qinfos) {
    if (qitem.name.rfind("data_request_") == 0) {
      try {
        datareqsink_t temp(qitem.inst);
      } catch (const ers::Issue &excpt) {
        throw InvalidQueueFatalError(ERS_HERE, get_name(), qitem.name, excpt);
      }
    }
  }

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)
      << get_name() << ": Exiting init() method";
}

void TriggerRecordBuilder::get_info(opmonlib::InfoCollector &ci,
                                    int /*level*/) {

  triggerrecordbuilderinfo::Info i;

  i.pending_trigger_decisions = m_trigger_decisions_counter.load();
  i.fragments_in_the_book = m_fragment_counter.load();
  i.pending_fragments = m_pending_fragment_counter.load();
  i.timed_out_trigger_records = m_timed_out_trigger_records.load();
  i.unexpected_fragments = m_unexpected_fragments.load();
  i.lost_fragments = m_lost_fragments.load();
  i.invalid_requests = m_invalid_requests.load();
  i.duplicated_trigger_ids = m_duplicated_trigger_ids.load();
  i.sleep_counter = m_sleep_counter.exchange(0);
  i.loop_counter = m_loop_counter.exchange(0);

  auto time = m_trigger_record_time.exchange(0.);
  auto n_triggers = m_completed_trigger_records.exchange(0);

  if ( n_triggers > 0 ) {
    i.average_millisecond_per_trigger = time / (metric_ratio_type)n_triggers ;
  }

  auto td_width = m_trigger_decision_width.exchange(0);
  auto n_td  = m_received_trigger_decisions.exchange(0);

  if ( n_td > 0 ) {
    i.average_decision_width = td_width / (metric_ratio_type) n_td ;
  }

  auto dr_width = m_data_request_width.exchange(0) ;
  auto n_dr     = m_generated_data_requests.exchange(0);

  if ( n_dr > 0 ) {
    i.average_data_request_width = dr_width /  (metric_ratio_type) n_dr ;
  }

  ci.add(i);
}

void TriggerRecordBuilder::do_conf(const data_t &payload) {
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)
      << get_name() << ": Entering do_conf() method";

  m_map_geoid_queues.clear();

  triggerrecordbuilder::ConfParams parsed_conf =
      payload.get<triggerrecordbuilder::ConfParams>();

  for (auto const &entry : parsed_conf.map) {

    dataformats::GeoID::SystemType type =
        dataformats::GeoID::string_to_system_type(entry.system);

    if (type == dataformats::GeoID::SystemType::kInvalid) {
      throw InvalidSystemType(ERS_HERE, entry.system);
    }

    dataformats::GeoID key;
    key.system_type = type;
    key.region_id = entry.region;
    key.element_id = entry.element;
    m_map_geoid_queues[key] = entry.queueinstance;
  }

  m_trigger_timeout = duration_type(parsed_conf.trigger_record_timeout_ms);

  m_loop_sleep = m_queue_timeout =
      std::chrono::milliseconds(parsed_conf.general_queue_timeout);

  if (m_map_geoid_queues.size() > 1) {
    m_loop_sleep /= (2. + log2(m_map_geoid_queues.size()));
    if (m_loop_sleep.count() == 0)
      m_loop_sleep = m_queue_timeout =
          std::chrono::milliseconds(parsed_conf.general_queue_timeout);
  }

  TLOG() << get_name() << ": timeouts (ms): queue = " << m_queue_timeout.count()
         << ", loop = " << m_loop_sleep.count();
  m_max_time_window = parsed_conf.max_time_window;

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)
    << get_name() << ": Exiting do_conf() method";
}
  
void TriggerRecordBuilder::do_start(const data_t & args) {
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)
      << get_name() << ": Entering do_start() method";

  m_run_number.reset( new const dataformats::run_number_t(args.at("run").get<dataformats::run_number_t>() ) );

  m_thread.start_working_thread(get_name());
  TLOG() << get_name() << " successfully started";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)
      << get_name() << ": Exiting do_start() method";
}

void TriggerRecordBuilder::do_stop(const data_t & /*args*/) {
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)
      << get_name() << ": Entering do_stop() method";
  m_thread.stop_working_thread();
  TLOG() << get_name() << " successfully stopped";
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)
      << get_name() << ": Exiting do_stop() method";
}

void TriggerRecordBuilder::do_work(std::atomic<bool> &running_flag) {
  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)
      << get_name() << ": Entering do_work() method";
  // uint32_t receivedCount = 0;

  // clean books from possible previous memory
  m_trigger_records.clear();
  m_trigger_decisions_counter.store(0);
  m_pending_fragment_counter.store(0);
  m_fragment_counter.store(0);
  m_timed_out_trigger_records.store(0);
  m_unexpected_fragments.store(0);
  m_lost_fragments.store(0);
  m_invalid_requests.store(0);
  m_duplicated_trigger_ids.store(0);

  // allocate queues
  trigger_decision_source_t decision_source(m_trigger_decision_source_name);
  trigger_record_sink_t record_sink(m_trigger_record_sink_name);
  std::vector<std::unique_ptr<fragment_source_t>> frag_sources;
  for (unsigned int i = 0; i < m_fragment_source_names.size(); ++i) {
    frag_sources.push_back(std::unique_ptr<fragment_source_t>(
        new fragment_source_t(m_fragment_source_names[i])));
  }

  datareqsinkmap_t request_sinks;
  for (auto const &entry : m_map_geoid_queues) {
    request_sinks[entry.first] =
        std::unique_ptr<datareqsink_t>(new datareqsink_t(entry.second));
  }

  bool run_again = false;

  while (running_flag.load() || run_again) {

    bool book_updates = false;

    // read decision requests
    while (decision_source.can_pop()) {

      dfmessages::TriggerDecision temp_dec;

      try {

        // get the trigger decision
        decision_source.pop(temp_dec, m_queue_timeout);

      } catch (const dunedaq::appfwk::QueueTimeoutExpired &excpt) {
        continue;
      }


      if ( temp_dec.run_number != *m_run_number ) {
	ers::error( UnexpectedTriggerDecision( ERS_HERE, 
					       temp_dec.trigger_number, 
					       temp_dec.run_number, 
					       *m_run_number ) );
	continue ;
      }

      book_updates = create_trigger_records_and_dispatch( temp_dec, request_sinks, running_flag) > 0 ;
      
      break;
      
    } // while loop, so that we can pop a trigger decision
    
    // read the fragments queues
    bool new_fragments = read_fragments(frag_sources);

    //-------------------------------------------------
    // Check if trigger records are complete or timedout
    // and create dedicated record
    //--------------------------------------------------

    if (new_fragments) {

      TLOG_DEBUG(TLVL_BOOKKEEPING)
          << "Bookeeping status: " << m_trigger_records.size()
          << " trigger records in progress ";

      std::vector<TriggerId> complete;
      for (const auto &tr : m_trigger_records) {

        auto comp_size = tr.second.second->get_fragments_ref().size();
        auto requ_size =
            tr.second.second->get_header_ref().get_num_requested_components();
        std::ostringstream message;
        message << tr.first << " with " << comp_size << '/' << requ_size
                << " components";

        if (comp_size == requ_size) {

          message << ": complete";
          complete.push_back(tr.first);
        }

        TLOG_DEBUG(TLVL_BOOKKEEPING) << message.str();

      } // loop over TRs to check if they are complete

      //------------------------------------------------
      // Create TriggerRecords and send them
      //-----------------------------------------------

      for (const auto &id : complete) {

        send_trigger_record(id, record_sink, running_flag);

      } // loop over compled trigger id

    } // if books were updated

    //-------------------------------------------------
    // Check if some fragments are obsolete
    //--------------------------------------------------
    book_updates |= check_stale_requests(record_sink, running_flag);

    run_again = book_updates || new_fragments;

    if (!run_again) {
      if (running_flag.load()) {
        ++m_sleep_counter;
        std::this_thread::sleep_for(m_loop_sleep);
      }
    } else {
      ++m_loop_counter;
    }

  } // working loop

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)
      << get_name() << ": Starting draining phase ";
  std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();

  // //-------------------------------------------------
  // // Here we drain what has been left from the running condition
  // //--------------------------------------------------

  // create all possible trigger record
  std::vector<TriggerId> triggers;
  for (const auto &entry : m_trigger_records) {
    triggers.push_back(entry.first);
  }

  // create the trigger record and send it
  for (const auto &t : triggers) {
    send_trigger_record(t, record_sink, running_flag);
  }

  std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();

  std::chrono::duration<double> time_span =
      std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);

  std::ostringstream oss_summ;
  oss_summ << ": Exiting the do_work() method, " << m_trigger_records.size()
           << " remaining Trigger Records" << std::endl
           << "Draining took : " << time_span.count() << " s";
  TLOG() << ProgressUpdate(ERS_HERE, get_name(), oss_summ.str());

  TLOG_DEBUG(TLVL_ENTER_EXIT_METHODS)
      << get_name() << ": Exiting do_work() method";
} // NOLINT(readability/fn_size)

bool TriggerRecordBuilder::read_fragments(fragment_sources_t &frag_sources,
                                          bool drain) {

  bool new_fragments = false;

  //-------------------------------------------------
  // Try to get Fragments from every queue
  //--------------------------------------------------

  for (unsigned int j = 0; j < frag_sources.size(); ++j) {

    while (frag_sources[j]->can_pop()) {

      std::unique_ptr<dataformats::Fragment> temp_fragment;

      try {
        frag_sources[j]->pop(temp_fragment, m_queue_timeout);

      } catch (const dunedaq::appfwk::QueueTimeoutExpired &excpt) {
        // it is perfectly reasonable that there might be no data in the queue
        // some fraction of the times that we check, so we just continue on and
        // try again
        continue;
      }

      new_fragments = true;

      TriggerId temp_id(*temp_fragment);
      bool requested = false;

      auto it = m_trigger_records.find(temp_id);

      if (it != m_trigger_records.end()) {

        // check if the fragment has a GeoId that was desired
        dataformats::TriggerRecordHeader &header =
            it->second.second->get_header_ref();

        for (size_t i = 0; i < header.get_num_requested_components(); ++i) {

          const dataformats::ComponentRequest &request = header[i];
          if (request.component == temp_fragment->get_element_id()) {
            requested = true;
            break;
          }

        } // request loop

      } // if there is a corresponding trigger ID entry in the boook

      if (requested) {
        it->second.second->add_fragment(std::move(temp_fragment));
        ++m_fragment_counter;
        --m_pending_fragment_counter;
      } else {
        ers::error(UnexpectedFragment(ERS_HERE, temp_id,
                                      temp_fragment->get_fragment_type_code(),
                                      temp_fragment->get_element_id()));
        ++m_unexpected_fragments;
      }

      if (!drain)
        break;

    } // while loop over the j-th queue

  } // queue loop

  return new_fragments;
}

TriggerRecordBuilder::trigger_record_ptr_t
TriggerRecordBuilder::extract_trigger_record(const TriggerId &id) {

  auto it = m_trigger_records.find(id);

  trigger_record_ptr_t temp = std::move(it->second.second);

  auto time = clock_type::now();
  auto duration = time - it->second.first;

  m_trigger_record_time +=
      std::chrono::duration_cast<duration_type>(duration).count();
  ++m_completed_trigger_records;

  m_trigger_records.erase(it);

  --m_trigger_decisions_counter;
  m_fragment_counter -= temp->get_fragments_ref().size();

  auto missing_fragments =
      temp->get_header_ref().get_num_requested_components() -
      temp->get_fragments_ref().size();

  if (missing_fragments > 0) {

    m_lost_fragments += missing_fragments;
    m_pending_fragment_counter -= missing_fragments;
    temp->get_header_ref().set_error_bit(TriggerRecordErrorBits::kIncomplete,
                                         true);

    TLOG() << get_name()
           << " sending incomplete TriggerRecord downstream at Stop time "
           << "(trigger/run_number=" << id << ", "
           << temp->get_fragments_ref().size() << " of "
           << temp->get_header_ref().get_num_requested_components()
           << " fragments included)";
  }

  return temp;
}

unsigned int 
TriggerRecordBuilder::create_trigger_records_and_dispatch( const dfmessages::TriggerDecision& td,
							   datareqsinkmap_t& sinks,
							   std::atomic<bool>& running ) {

  unsigned int new_tr_counter = 0 ;

  // check the whole time window
  dataformats::timestamp_t begin = std::numeric_limits<dataformats::timestamp_t>::max();
  dataformats::timestamp_t end   = 0;

  for ( const auto & component : td.components ) {
    if ( component.window_begin < begin ) 
      begin = component.window_begin ;
    if ( component.window_end > end ) 
      end = component.window_end ; 
  }
  
  dataformats::timestamp_diff_t tot_width = end - begin ;
  dataformats::sequence_number_t max_sequence_number = m_max_time_window > 0 ? tot_width / m_max_time_window : 0 ;
  
  TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": trig_number " << td.trigger_number << ": run_number "
			      << td.run_number << ": trig_timestamp " << td.trigger_timestamp 
			      << " will have " << max_sequence_number + 1 << " sequences";
  
  ++m_received_trigger_decisions ;
  m_trigger_decision_width += tot_width ;

  // create the trigger records 
  for ( dataformats::sequence_number_t sequence = 0 ; 
	sequence <= max_sequence_number ; 
	++ sequence ) {
    
    dataformats::timestamp_t slice_begin = begin + sequence*m_max_time_window ; 
    dataformats::timestamp_t slice_end   = m_max_time_window > 0 ? std::min( slice_begin + m_max_time_window, end ) : end ; 
    
    // create the components cropped in time
    decltype( td.components ) slice_components;
    for ( const auto & component : td.components ) {
      
      if ( component.window_begin >= slice_end ) continue ;
      if ( component.window_end <= slice_begin ) continue ;
      
      dataformats::timestamp_t new_begin = std::max( slice_begin, component.window_begin ) ;
      dataformats::timestamp_t new_end   = std::min( slice_end, component.window_end ) ;

      dataformats::ComponentRequest temp( component.component, new_begin, new_end ) ;
      slice_components.push_back( temp ) ;

      ++m_generated_data_requests;
      m_data_request_width += new_end - new_begin ;
      
    }  // loop over component in trigger decision


    // if ( slice_components.empty() ) continue ;  MR: I would like to discuss this line with Kurt at least
    
    // create the book entry
    TriggerId slice_id(td, sequence);

    auto it = m_trigger_records.find(slice_id);
    if (it != m_trigger_records.end()) {
      ers::error(DuplicatedTriggerDecision(ERS_HERE, slice_id));
      ++m_duplicated_trigger_ids;
      continue ;
    }

    // create trigger record for the slice
    auto & entry = m_trigger_records[slice_id] = std::make_pair(clock_type::now(), trigger_record_ptr_t());;
    trigger_record_ptr_t &trp = entry.second; 
    trp.reset(new dataformats::TriggerRecord(slice_components));
    dataformats::TriggerRecord& tr = *trp;

    tr.get_header_ref().set_trigger_number(td.trigger_number);
    tr.get_header_ref().set_sequence_number(sequence);
    tr.get_header_ref().set_max_sequence_number(max_sequence_number);
    tr.get_header_ref().set_run_number(td.run_number);
    tr.get_header_ref().set_trigger_timestamp(td.trigger_timestamp);
    tr.get_header_ref().set_trigger_type(td.trigger_type);

    m_trigger_decisions_counter++;
    m_pending_fragment_counter += slice_components.size() ;
    ++ new_tr_counter ;

    // create and send the requests 
    TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Trigger Decision components: " << td.components.size();

    for ( const auto & component : slice_components ) {
      
      dfmessages::DataRequest dataReq;
      dataReq.trigger_number = td.trigger_number;
      dataReq.sequence_number = sequence;
      dataReq.run_number = td.run_number;
      dataReq.trigger_timestamp = td.trigger_timestamp;
      dataReq.readout_type = td.readout_type;
      dataReq.window_begin = component.window_begin;
      dataReq.window_end = component.window_end;
      
      TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": TR " << slice_id 
				  << ": trig_timestamp " << dataReq.trigger_timestamp 
				  << ": GeoID " << component.component
				  << ": window [" << dataReq.window_begin
				  << ", " << dataReq.window_end << ']';
      

      dispatch_data_requests( dataReq, component.component, sinks, running);

      
    } // loop loop over component in the slice

  } // sequence loop 
  
  return new_tr_counter ;
}

bool
TriggerRecordBuilder::dispatch_data_requests(const dfmessages::DataRequest& dr, 
					     const dataformats::GeoID & geo,
					     datareqsinkmap_t& sinks, std::atomic<bool>& running) const
  
{
  
  // find the queue for geoid_req in the map
  auto it_req = sinks.find(geo);
  if (it_req == sinks.end()) {
    // if geoid request is not valid. then trhow error and continue
    ers::error(dunedaq::dfmodules::UnknownGeoID(ERS_HERE, geo));
    ++m_invalid_requests;
    return false ;
  }
  
  // get the queue from map element
  auto& queue = it_req->second;
  
  bool wasSentSuccessfully = false;
  do {
    TLOG_DEBUG(TLVL_WORK_STEPS) << get_name() << ": Pushing the DataRequest from trigger number "
				<< dr.trigger_number << " onto output queue :" << queue->get_name();
    
    // push data request into the corresponding queue
    try {
      queue->push(dr, m_queue_timeout);
      wasSentSuccessfully = true;
    } catch (const dunedaq::appfwk::QueueTimeoutExpired& excpt) {
      std::ostringstream oss_warn;
      oss_warn << "push to output queue \"" << queue->get_name() << "\"";
      ers::warning(dunedaq::appfwk::QueueTimeoutExpired(
							ERS_HERE,
							get_name(),
							oss_warn.str(),
							std::chrono::duration_cast<std::chrono::milliseconds>(m_queue_timeout).count()));
    }
  } while (!wasSentSuccessfully && running.load());


  return wasSentSuccessfully;
}

bool TriggerRecordBuilder::send_trigger_record(const TriggerId &id,
                                               trigger_record_sink_t &sink,
                                               std::atomic<bool> &running) {

  trigger_record_ptr_t temp_record(extract_trigger_record(id));

  bool wasSentSuccessfully = false;
  while (!wasSentSuccessfully) {
    try {
      sink.push(std::move(temp_record), m_queue_timeout);
      wasSentSuccessfully = true;
    } catch (const dunedaq::appfwk::QueueTimeoutExpired &excpt) {
      std::ostringstream oss_warn;
      oss_warn << "push to output queue \"" << get_name() << "\"";
      ers::warning(dunedaq::appfwk::QueueTimeoutExpired(
          ERS_HERE, sink.get_name(), oss_warn.str(),
          std::chrono::duration_cast<std::chrono::milliseconds>(m_queue_timeout)
              .count()));
    }

    if (!running.load())
      break;
  } // push while loop

  return wasSentSuccessfully;
}

bool TriggerRecordBuilder::check_stale_requests(trigger_record_sink_t &sink,
                                                std::atomic<bool> &running) {

  bool book_updates = false;

  // -----------------------------------------------
  // optionally send over stale trigger records
  // -----------------------------------------------

  if (m_trigger_timeout.count() > 0) {

    std::vector<TriggerId> stale_triggers;

    for (auto it = m_trigger_records.begin(); it != m_trigger_records.end();
         ++it) {

      dataformats::TriggerRecord &tr = *it->second.second;

      auto tr_time = clock_type::now() - it->second.first;

      if (tr_time > m_trigger_timeout) {

        ers::error(TimedOutTriggerDecision(
            ERS_HERE, it->first, tr.get_header_ref().get_trigger_timestamp()));

        // mark trigger record for seding
        stale_triggers.push_back(it->first);
        ++m_timed_out_trigger_records;

        book_updates = true;
      }

    } // trigger record loop

    // create the trigger record and send it
    for (const auto &t : stale_triggers) {
      send_trigger_record(t, sink, running);
    }

  } //  m_trigger_timeout > 0

  return book_updates;
}

} // namespace dfmodules
} // namespace dunedaq

DEFINE_DUNE_DAQ_MODULE(dunedaq::dfmodules::TriggerRecordBuilder)
