/**
 * @file HDF5Write_test.cxx Application that tests and demonstrates
 * the write functionality of the HDF5DataStore class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/DataStore.hpp"

// We need the actual HDF5DataStore.hpp plugin header so the unit tests can access its exceptions
#include "../plugins/HDF5DataStore.hpp" // NOLINT(build/include_path)

#include "appmodel/ConfigurationHelper.hpp"
#include "appmodel/DataStoreConf.hpp"
#include "appmodel/DataWriterConf.hpp"
#include "appmodel/DataWriterModule.hpp"
#include "appmodel/FilenameParams.hpp"
#include "appmodel/HDF5DataStoreConf.hpp"
#include "confmodel/DetectorConfig.hpp"
#include "confmodel/Session.hpp"
#include "detdataformats/DetID.hpp"
#include "dfmodules/opmon/DataStore.pb.h"
#include "hdf5libs/HDF5RawDataFile.hpp"
#include "hdf5libs/HDF5SourceIDHandler.hpp"
#include "opmonlib/TestOpMonManager.hpp"
#include "opmonlib/Utils.hpp"
#include "rawdatafilelibs/BackendFactory.hpp"

#define BOOST_TEST_MODULE HDF5Write_test // NOLINT

#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/test/unit_test.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

using namespace dunedaq::dfmodules;

std::vector<std::string>
get_files_matching_pattern(const std::string& path, const std::string& pattern)
{
  std::regex regex_search_pattern(pattern);
  std::vector<std::string> file_list;
  for (const auto& entry : std::filesystem::directory_iterator(path)) {
    if (std::regex_match(entry.path().filename().string(), regex_search_pattern)) {
      file_list.push_back(entry.path());
    }
  }
  return file_list;
}

std::vector<std::string>
delete_files_matching_pattern(const std::string& path, const std::string& pattern)
{
  std::regex regex_search_pattern(pattern);
  std::vector<std::string> file_list;
  for (const auto& entry : std::filesystem::directory_iterator(path)) {
    if (std::regex_match(entry.path().filename().string(), regex_search_pattern)) {
      if (std::filesystem::remove(entry.path())) {
        file_list.push_back(entry.path());
      }
    }
  }
  return file_list;
}

dunedaq::daqdataformats::TriggerRecord
create_trigger_record(int trig_num, int fragment_size, int element_count, int run_number = 53)
{
  const dunedaq::daqdataformats::SourceID::Subsystem stype_to_use =
    dunedaq::daqdataformats::SourceID::Subsystem::kDetectorReadout;

  // setup our dummy_data
  std::vector<char> dummy_vector(fragment_size);
  char* dummy_data = dummy_vector.data();

  // get a timestamp for this trigger
  uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>( // NOLINT(build/unsigned)
                  system_clock::now().time_since_epoch())
                  .count();

  // create TriggerRecordHeader, with its element_count component requests (a lone
  // TriggerRecordHeaderData claiming them would make the header read past it)
  std::vector<dunedaq::daqdataformats::ComponentRequest> component_requests(element_count);
  dunedaq::daqdataformats::TriggerRecordHeader trh(component_requests);
  trh.set_trigger_number(trig_num);
  trh.set_trigger_timestamp(ts);
  trh.set_run_number(run_number);
  trh.set_sequence_number(0);
  trh.set_max_sequence_number(1);
  trh.set_element_id(dunedaq::daqdataformats::SourceID(dunedaq::daqdataformats::SourceID::Subsystem::kTRBuilder, 0));

  // create out TriggerRecord
  dunedaq::daqdataformats::TriggerRecord tr(trh);

  // loop over elements
  for (int ele_num = 0; ele_num < element_count; ++ele_num) {

    // create our fragment
    dunedaq::daqdataformats::FragmentHeader fh;
    fh.trigger_number = trig_num;
    fh.trigger_timestamp = ts;
    fh.window_begin = ts - 10;
    fh.window_end = ts;
    fh.run_number = run_number;
    fh.sequence_number = 0;
    fh.fragment_type =
      static_cast<dunedaq::daqdataformats::fragment_type_t>(dunedaq::daqdataformats::FragmentType::kWIB);
    // NOLINTNEXTLINE(build/unsigned)
    fh.detector_id = static_cast<uint16_t>(dunedaq::detdataformats::DetID::Subdetector::kHD_TPC);
    fh.element_id = dunedaq::daqdataformats::SourceID(stype_to_use, ele_num);
    std::unique_ptr<dunedaq::daqdataformats::Fragment> frag_ptr(
      new dunedaq::daqdataformats::Fragment(dummy_data, fragment_size));
    frag_ptr->set_header_fields(fh);

    // add fragment to TriggerRecord
    tr.add_fragment(std::move(frag_ptr));

  } // end loop over elements

  return tr;
}

struct CfgFixture
{
  explicit CfgFixture(std::string sessionName)
  {
    TLOG_DEBUG(4) << "Creating CfgFixture";
    setenv("DUNEDAQ_SESSION", sessionName.c_str(), 1);
    std::string oksConfig = "oksconflibs:test/config/hdf5write_test.data.xml";
    std::string appName = "TestApp";
    cfgMgr = std::make_shared<dunedaq::appfwk::ConfigurationManager>(oksConfig, appName, sessionName);
    TLOG_DEBUG(4) << "Done with CfgFixture";
  }

  const dunedaq::confmodel::DetectorConfig* get_detector_config()
  {
    return cfgMgr->get_session()->get_detector_configuration();
  }

  std::shared_ptr<dunedaq::appfwk::ConfigurationManager> cfgMgr;
};

BOOST_AUTO_TEST_SUITE(HDF5Write_test)

BOOST_AUTO_TEST_CASE(NullConfiguration)
{
  BOOST_CHECK_THROW(make_data_store("HDF5DataStore", "dummy", nullptr, "dummy"),
                    dunedaq::dfmodules::FileDataStoreBadConfiguration);
}

BOOST_AUTO_TEST_CASE(WriteEventFiles)
{
  std::string file_path(std::filesystem::temp_directory_path());

  const int trigger_count = 5;
  const int apa_count = 3;
  const int link_count = 1;
  const int fragment_size = 10 + sizeof(dunedaq::daqdataformats::FragmentHeader);

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = "hdf5writetest.*\\.hdf5(\\.writing)?";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  CfgFixture cfg("test-session-3-1");
  auto data_writer_conf = cfg.cfgMgr->get_dal<dunedaq::appmodel::DataWriterModule>("dwm-01")->get_configuration();
  auto data_store_conf = data_writer_conf->get_data_store_params();

  auto data_store_conf_obj = data_store_conf->config_object();
  data_store_conf_obj.set_by_val<std::string>("directory_path", file_path);
  data_store_conf_obj.set_by_val<std::string>("mode", "one-event-per-file");

  auto data_store_ptr = make_data_store(data_store_conf->get_type(), data_store_conf->UID(), cfg.cfgMgr, "dwm-01");

  // write several events, each with several fragments
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number)
    data_store_ptr->write(create_trigger_record(trigger_number, fragment_size, link_count * apa_count));

  data_store_ptr.reset(); // explicit destruction

  // check that the expected number of files was created
  std::string search_pattern = "hdf5writetest.*\\.hdf5";
  std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), trigger_count);

  // clean up the files that were created
  file_list = delete_files_matching_pattern(file_path, delete_pattern);
  delete_files_matching_pattern(file_path, "HardwareMap.*\\.txt");
  BOOST_REQUIRE_EQUAL(file_list.size(), trigger_count);
}

BOOST_AUTO_TEST_CASE(WriteOneFile)
{
  std::string file_path(std::filesystem::temp_directory_path());

  const int trigger_count = 5;
  const int apa_count = 3;
  const int link_count = 1;
  const int fragment_size = 10 + sizeof(dunedaq::daqdataformats::FragmentHeader);

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = "hdf5writetest.*\\.hdf5(\\.writing)?";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  CfgFixture cfg("test-session-3-1");
  auto data_writer_conf = cfg.cfgMgr->get_dal<dunedaq::appmodel::DataWriterModule>("dwm-01")->get_configuration();
  auto data_store_conf = data_writer_conf->get_data_store_params();

  auto data_store_conf_obj = data_store_conf->config_object();
  data_store_conf_obj.set_by_val<std::string>("directory_path", file_path);

  auto data_store_ptr = make_data_store(data_store_conf->get_type(), data_store_conf->UID(), cfg.cfgMgr, "dwm-01");

  // write several events, each with several fragments
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number)
    data_store_ptr->write(create_trigger_record(trigger_number, fragment_size, apa_count * link_count));

  data_store_ptr.reset(); // explicit destruction

  // check that the expected number of files was created
  std::string search_pattern = "hdf5writetest.*\\.hdf5";
  std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);

  // clean up the files that were created
  file_list = delete_files_matching_pattern(file_path, delete_pattern);
  delete_files_matching_pattern(file_path, "HardwareMap.*\\.txt");
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);
}

// The time in the file name and the creation_timestamp attribute come from the same clock reading
BOOST_AUTO_TEST_CASE(FileNameMatchesCreationTimestamp)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string delete_pattern = "hdf5writetest.*\\.hdf5(\\.writing)?";
  delete_files_matching_pattern(file_path, delete_pattern);

  CfgFixture cfg("test-session-3-1");
  auto data_writer_conf = cfg.cfgMgr->get_dal<dunedaq::appmodel::DataWriterModule>("dwm-01")->get_configuration();
  auto data_store_conf = data_writer_conf->get_data_store_params();
  auto data_store_conf_obj = data_store_conf->config_object();
  data_store_conf_obj.set_by_val<std::string>("directory_path", file_path);
  data_store_conf_obj.set_by_val<bool>("disable_unique_filename_suffix", false);

  auto data_store_ptr = make_data_store(data_store_conf->get_type(), data_store_conf->UID(), cfg.cfgMgr, "dwm-01");
  data_store_ptr->write(create_trigger_record(1, 10 + sizeof(dunedaq::daqdataformats::FragmentHeader), 2));
  data_store_ptr.reset();

  std::vector<std::string> file_list = get_files_matching_pattern(file_path, delete_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);

  std::smatch match;
  std::regex timestamp_pattern(".*_([0-9]{8}T[0-9]{6})\\.hdf5$");
  BOOST_REQUIRE(std::regex_match(file_list[0], match, timestamp_pattern));

  dunedaq::hdf5libs::HDF5RawDataFile file(file_list[0]);
  auto creation_timestamp_ms = file.get_attribute<size_t>("creation_timestamp");
  std::string expected =
    boost::posix_time::to_iso_string(boost::posix_time::from_time_t(static_cast<time_t>(creation_timestamp_ms / 1000)));
  BOOST_REQUIRE_EQUAL(match[1].str(), expected);

  // the run-context attributes are still written
  auto names = file.get_attribute_names();
  for (auto const& name : { "operational_environment", "offline_data_stream", "run_was_for_test_purposes" }) {
    BOOST_REQUIRE(std::find(names.begin(), names.end(), name) != names.end());
  }
  file.close();

  delete_files_matching_pattern(file_path, delete_pattern);
}

// The file stores the session's SourceID->GeoID map, built by appmodel's ConfigurationHelper. That
// gives the same map as the deprecated hdf5libs function it replaces.
BOOST_AUTO_TEST_CASE(GeoIDMapStoredInFile)
{
  for (auto const& session_name : { "test-session-3-1", "test-session-5-1", "test-session-5-10" }) {
    CfgFixture cfg(session_name);
    auto geo_id_map = dunedaq::appmodel::ConfigurationHelper(cfg.cfgMgr->get_session()).get_source_id_geo_id_map();
    BOOST_REQUIRE(!geo_id_map.empty());
    BOOST_REQUIRE(geo_id_map.to_encoded() ==
                  dunedaq::hdf5libs::HDF5SourceIDHandler::make_source_id_geo_id_map(*cfg.cfgMgr->get_session()));
  }

  std::string file_path(std::filesystem::temp_directory_path());
  std::string delete_pattern = "hdf5writetest.*\\.hdf5(\\.writing)?";
  delete_files_matching_pattern(file_path, delete_pattern);

  CfgFixture cfg("test-session-3-1");
  auto data_writer_conf = cfg.cfgMgr->get_dal<dunedaq::appmodel::DataWriterModule>("dwm-01")->get_configuration();
  auto data_store_conf = data_writer_conf->get_data_store_params();
  auto data_store_conf_obj = data_store_conf->config_object();
  data_store_conf_obj.set_by_val<std::string>("directory_path", file_path);

  auto data_store_ptr = make_data_store(data_store_conf->get_type(), data_store_conf->UID(), cfg.cfgMgr, "dwm-01");
  data_store_ptr->write(create_trigger_record(1, 10 + sizeof(dunedaq::daqdataformats::FragmentHeader), 2));
  data_store_ptr.reset();

  std::vector<std::string> file_list = get_files_matching_pattern(file_path, delete_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);
  auto reader = dunedaq::rawdatafilelibs::open_raw_data_file(file_list[0]);
  BOOST_REQUIRE(reader->get_geo_id_map() ==
                dunedaq::appmodel::ConfigurationHelper(cfg.cfgMgr->get_session()).get_source_id_geo_id_map());
  reader->close();

  delete_files_matching_pattern(file_path, delete_pattern);
}

// An HDF5DataStoreConf gives the HDF5 layout through hdf5_file_layout_params (here a TimeSlice layout)
// An HDF5DataStoreConf without hdf5_file_layout_params (and no deprecated DataStoreConf layout) uses
// the standard layout for the record type
BOOST_AUTO_TEST_CASE(HDF5DataStoreDefaultLayout)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string delete_pattern = "hdf5writetest.*\\.hdf5(\\.writing)?";
  delete_files_matching_pattern(file_path, delete_pattern);

  CfgFixture cfg("test-session-3-1");
  // reached the way DataWriterModule reaches it (see HDF5DataStoreConfLayout)
  auto data_writer_conf =
    cfg.cfgMgr->get_dal<dunedaq::appmodel::DataWriterModule>("dwm-default-layout")->get_configuration();
  auto data_store_conf = data_writer_conf->get_data_store_params();
  BOOST_REQUIRE(data_store_conf != nullptr);
  auto data_store_conf_obj = data_store_conf->config_object();
  data_store_conf_obj.set_by_val<std::string>("directory_path", file_path);
  auto hdf5_conf = data_store_conf->cast<dunedaq::appmodel::HDF5DataStoreConf>();
  BOOST_REQUIRE(hdf5_conf != nullptr);
  BOOST_REQUIRE(hdf5_conf->get_hdf5_file_layout_params() == nullptr);
  BOOST_REQUIRE(data_store_conf->get_file_layout_params() == nullptr);

  auto data_store_ptr = make_data_store(data_store_conf->get_type(), data_store_conf->UID(), cfg.cfgMgr, "dwm-01");
  data_store_ptr->write(create_trigger_record(1, 10 + sizeof(dunedaq::daqdataformats::FragmentHeader), 2));
  data_store_ptr.reset();

  std::vector<std::string> file_list = get_files_matching_pattern(file_path, "hdf5writetest.*\\.hdf5");
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);
  auto reader = dunedaq::rawdatafilelibs::open_raw_data_file(file_list[0]);
  BOOST_REQUIRE(reader->get_record_type() == dunedaq::rawdatafilelibs::RecordType::kTriggerRecord);
  BOOST_REQUIRE_EQUAL(reader->get_trigger_record(dunedaq::daqdataformats::RecordID{ 1 }).get_fragments_ref().size(), 2);
  reader->close();

  delete_files_matching_pattern(file_path, delete_pattern);
}

BOOST_AUTO_TEST_CASE(HDF5DataStoreConfLayout)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string delete_pattern = "hdf5writetest.*\\.hdf5(\\.writing)?";
  delete_files_matching_pattern(file_path, delete_pattern);

  CfgFixture cfg("test-session-3-1");
  // reached the way DataWriterModule reaches it, through its DataWriterConf; the directory_path change has to come
  // before anything reads the object's attributes (conffwk DAL objects cache them)
  auto data_writer_conf = cfg.cfgMgr->get_dal<dunedaq::appmodel::DataWriterModule>("dwm-ts")->get_configuration();
  auto data_store_conf = data_writer_conf->get_data_store_params();
  BOOST_REQUIRE(data_store_conf != nullptr);
  auto data_store_conf_obj = data_store_conf->config_object();
  data_store_conf_obj.set_by_val<std::string>("directory_path", file_path);
  BOOST_REQUIRE(data_store_conf->cast<dunedaq::appmodel::HDF5DataStoreConf>() != nullptr);

  auto data_store_ptr = make_data_store(data_store_conf->get_type(), data_store_conf->UID(), cfg.cfgMgr, "dwm-01");
  dunedaq::daqdataformats::TimeSlice timeslice{ 77, 999 }; // timeslice #, run #
  data_store_ptr->write(timeslice);
  data_store_ptr.reset();

  std::vector<std::string> file_list = get_files_matching_pattern(file_path, "hdf5writetest.*\\.hdf5");
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);
  auto reader = dunedaq::rawdatafilelibs::open_raw_data_file(file_list[0]);
  BOOST_REQUIRE(reader->get_record_type() == dunedaq::rawdatafilelibs::RecordType::kTimeSlice);
  BOOST_REQUIRE(reader->contains(dunedaq::daqdataformats::RecordID{ 77 }));
  BOOST_REQUIRE_EQUAL(reader->get_metadata().run_number, 0); // prepare_for_run wasn't called
  reader->close();

  delete_files_matching_pattern(file_path, delete_pattern);
}

BOOST_AUTO_TEST_CASE(CheckWritingSuffix)
{
  std::string file_path(std::filesystem::temp_directory_path());

  const int trigger_count = 5;
  const int apa_count = 3;
  const int link_count = 1;
  const int fragment_size = 10 + sizeof(dunedaq::daqdataformats::FragmentHeader);

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = "hdf5writetest.*\\.hdf5(\\.writing)?";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  CfgFixture cfg("test-session-3-1");
  auto data_writer_conf = cfg.cfgMgr->get_dal<dunedaq::appmodel::DataWriterModule>("dwm-01")->get_configuration();
  auto data_store_conf = data_writer_conf->get_data_store_params();

  auto data_store_conf_obj = data_store_conf->config_object();
  data_store_conf_obj.set_by_val<std::string>("directory_path", file_path);

  auto data_store_ptr = make_data_store(data_store_conf->get_type(), data_store_conf->UID(), cfg.cfgMgr, "dwm-01");

  // write several events, each with several fragments
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number) {
    data_store_ptr->write(create_trigger_record(trigger_number, fragment_size, apa_count * link_count));

    // check that the .writing file is there
    std::string search_pattern = "hdf5writetest.*\\.writing";
    std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
    BOOST_REQUIRE_EQUAL(file_list.size(), 1);
  }

  data_store_ptr.reset(); // explicit destruction

  // check that the expected number of files was created
  std::string search_pattern = "hdf5writetest.*\\.writing";
  std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), 0);

  // clean up the files that were created
  file_list = delete_files_matching_pattern(file_path, delete_pattern);
  delete_files_matching_pattern(file_path, "HardwareMap.*\\.txt");
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);
}

BOOST_AUTO_TEST_CASE(NoDuplicateTimeSlices)
{
  std::string file_path(std::filesystem::temp_directory_path());

  CfgFixture cfg("test-session-3-1");
  auto data_writer_conf = cfg.cfgMgr->get_dal<dunedaq::appmodel::DataWriterModule>("dwm-01")->get_configuration();
  auto data_store_conf = data_writer_conf->get_data_store_params();

  auto data_store_conf_obj = data_store_conf->config_object();
  data_store_conf_obj.set_by_val<std::string>("directory_path", file_path);

  // TimeSlices need a TimeSlice file layout, as TP-stream writers are configured with
  auto layout_conf_obj = data_store_conf->get_file_layout_params()->config_object();
  layout_conf_obj.set_by_val<std::string>("record_name_prefix", "TimeSlice");
  layout_conf_obj.set_by_val<int32_t>("digits_for_sequence_number", 0);
  layout_conf_obj.set_by_val<std::string>("record_header_dataset_name", "TimeSliceHeader");

  auto data_store_ptr = make_data_store(data_store_conf->get_type(), data_store_conf->UID(), cfg.cfgMgr, "dwm-01");

  dunedaq::daqdataformats::TimeSlice timeslice{ 999, 999 }; // timeslice #, run #
  dunedaq::daqdataformats::TimeSlice identical_timeslice{ 999, 999 };

  data_store_ptr->write(timeslice);

  std::string search_pattern = "hdf5writetest.*\\.writing";
  std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);

  BOOST_CHECK_THROW(data_store_ptr->write(identical_timeslice), dunedaq::dfmodules::IgnorableDataStoreProblem);

  delete_files_matching_pattern(file_path, "hdf5writetest.*\\.hdf5");
}

BOOST_AUTO_TEST_CASE(WriteToBadOutputArea)
{
  std::string file_path("/this/path/does/not/exist");

  CfgFixture cfg("test-session-3-1");
  auto data_writer_conf = cfg.cfgMgr->get_dal<dunedaq::appmodel::DataWriterModule>("dwm-01")->get_configuration();
  auto data_store_conf = data_writer_conf->get_data_store_params();

  auto data_store_conf_obj = data_store_conf->config_object();
  data_store_conf_obj.set_by_val<std::string>("directory_path", file_path);
  auto data_store_ptr = make_data_store(data_store_conf->get_type(), data_store_conf->UID(), cfg.cfgMgr, "dwm-01");

  BOOST_CHECK_THROW(data_store_ptr->prepare_for_run(1, true), dunedaq::dfmodules::InvalidOutputPath);
}

BOOST_AUTO_TEST_CASE(EnormousMaxFileSize)
{
  CfgFixture cfg("test-session-3-1");
  auto data_writer_conf = cfg.cfgMgr->get_dal<dunedaq::appmodel::DataWriterModule>("dwm-01")->get_configuration();
  auto data_store_conf = data_writer_conf->get_data_store_params();

  auto data_store_conf_obj = data_store_conf->config_object();
  data_store_conf_obj.set_by_val<size_t>("max_file_size", std::numeric_limits<size_t>::max());
  auto data_store_ptr = make_data_store(data_store_conf->get_type(), data_store_conf->UID(), cfg.cfgMgr, "dwm-01");
  BOOST_CHECK_THROW(data_store_ptr->prepare_for_run(1, true), dunedaq::dfmodules::InsufficientDiskSpace);
}

BOOST_AUTO_TEST_CASE(FileSizeLimitResultsInMultipleFiles)
{
  std::string file_path(std::filesystem::temp_directory_path());

  const int trigger_count = 15;
  const int apa_count = 5;
  const int link_count = 10;
  const int fragment_size = 10000;

  // 5 APAs times 10 links times 10000 bytes per fragment gives 500,000 bytes per TR
  // So, 15 TRs would give 7,500,000 bytes total.

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = "hdf5writetest.*\\.hdf5(\\.writing)?";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  CfgFixture cfg("test-session-5-10");
  auto data_writer_conf = cfg.cfgMgr->get_dal<dunedaq::appmodel::DataWriterModule>("dwm-01")->get_configuration();
  auto data_store_conf = data_writer_conf->get_data_store_params();

  auto data_store_conf_obj = data_store_conf->config_object();
  data_store_conf_obj.set_by_val<std::string>("directory_path", file_path);
  data_store_conf_obj.set_by_val<int>("max_file_size", 3000000); // goal is 6 events per file

  auto data_store_ptr = make_data_store(data_store_conf->get_type(), data_store_conf->UID(), cfg.cfgMgr, "dwm-01");

  // write several events, each with several fragments
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number)
    data_store_ptr->write(create_trigger_record(trigger_number, fragment_size, apa_count * link_count));

  data_store_ptr.reset(); // explicit destruction

  // check that the expected number of files was created
  std::string search_pattern = "hdf5writetest.*\\.hdf5";
  std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
  // 7,500,000 bytes stored in files of size 3,000,000 should result in three files.
  BOOST_REQUIRE_EQUAL(file_list.size(), 3);

  // clean up the files that were created
  file_list = delete_files_matching_pattern(file_path, delete_pattern);
  delete_files_matching_pattern(file_path, "HardwareMap.*\\.txt");
  BOOST_REQUIRE_EQUAL(file_list.size(), 3);
}

BOOST_AUTO_TEST_CASE(SmallFileSizeLimitDataBlockListWrite)
{
  std::string file_path(std::filesystem::temp_directory_path());

  const int trigger_count = 5;
  const int apa_count = 5;
  const int link_count = 1;
  const int fragment_size = 100000;

  // 5 APAs times 100000 bytes per fragment gives 500,000 bytes per TR

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = "hdf5writetest.*\\.hdf5(\\.writing)?";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  CfgFixture cfg("test-session-5-1");
  auto data_writer_conf = cfg.cfgMgr->get_dal<dunedaq::appmodel::DataWriterModule>("dwm-01")->get_configuration();
  auto data_store_conf = data_writer_conf->get_data_store_params();

  auto data_store_conf_obj = data_store_conf->config_object();
  data_store_conf_obj.set_by_val<std::string>("directory_path", file_path);
  data_store_conf_obj.set_by_val<int>("max_file_size", 150000); // ~1.5 Fragment, ~0.3 TR

  auto data_store_ptr = make_data_store(data_store_conf->get_type(), data_store_conf->UID(), cfg.cfgMgr, "dwm-01");

  // write several events, each with several fragments
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number)
    data_store_ptr->write(create_trigger_record(trigger_number, fragment_size, apa_count * link_count));

  data_store_ptr.reset(); // explicit destruction

  // check that the expected number of files was created
  std::string search_pattern = "hdf5writetest.*\\.hdf5";
  std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
  // each TriggerRecord should be stored in its own file
  BOOST_REQUIRE_EQUAL(file_list.size(), 5);

  // clean up the files that were created
  file_list = delete_files_matching_pattern(file_path, delete_pattern);
  delete_files_matching_pattern(file_path, "HardwareMap.*\\.txt");
  BOOST_REQUIRE_EQUAL(file_list.size(), 5);
}

namespace {
// Writes two runs of records through a DataStore, collecting its opmon counters after every write,
// and checks that they never underflow or carry over between runs or files (B2, B3). Exact equality
// with the size on disk isn't expected: closing a file adds attributes after the last size reading.
void
check_opmon_counters_over_runs_and_rollovers(const std::string& mode, size_t max_file_size, int records_per_run)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string delete_pattern = "hdf5writetest.*\\.hdf5(\\.writing)?";
  delete_files_matching_pattern(file_path, delete_pattern);

  const int element_count = 3;
  const int fragment_size = 100000;
  const size_t record_bytes = element_count * (fragment_size + sizeof(dunedaq::daqdataformats::FragmentHeader));
  // much larger than any honest per-interval or per-file count, much smaller than a wrapped uint64
  const uint64_t sane_bound = 100 * (max_file_size + record_bytes); // NOLINT(build/unsigned)

  CfgFixture cfg("test-session-3-1");
  auto data_writer_conf = cfg.cfgMgr->get_dal<dunedaq::appmodel::DataWriterModule>("dwm-01")->get_configuration();
  auto data_store_conf = data_writer_conf->get_data_store_params();
  auto data_store_conf_obj = data_store_conf->config_object();
  data_store_conf_obj.set_by_val<std::string>("directory_path", file_path);
  data_store_conf_obj.set_by_val<std::string>("mode", mode);
  data_store_conf_obj.set_by_val<uint64_t>("max_file_size", max_file_size); // NOLINT(build/unsigned)

  dunedaq::opmonlib::TestOpMonManager opmgr;
  auto data_store_ptr = make_data_store(data_store_conf->get_type(), data_store_conf->UID(), cfg.cfgMgr, "dwm-01");
  opmgr.register_node("data_store", data_store_ptr);

  auto collect = [&]() {
    opmgr.collect();
    auto entries = opmgr.get_backend_facility()->get_entries(std::regex(".*FileDataStoreInfo"));
    BOOST_REQUIRE_EQUAL(entries.size(), 1);
    return dunedaq::opmonlib::from_entry<dunedaq::dfmodules::opmon::FileDataStoreInfo>(entries.front());
  };
  collect(); // start from zero

  for (int run_number : { 53, 54 }) {
    data_store_ptr->prepare_for_run(run_number, true);
    uint64_t run_new_bytes = 0; // NOLINT(build/unsigned)
    for (int trigger_number = 1; trigger_number <= records_per_run; ++trigger_number) {
      data_store_ptr->write(create_trigger_record(trigger_number, fragment_size, element_count, run_number));
      auto info = collect();
      BOOST_TEST_INFO("mode " << mode << ", run " << run_number << ", record " << trigger_number);
      BOOST_REQUIRE(info.new_bytes_output() > 0);
      BOOST_REQUIRE(info.new_bytes_output() < sane_bound);
      // the current file never holds more than one record beyond the size limit, so nothing carried over
      BOOST_REQUIRE(info.bytes_in_file() > 0);
      BOOST_REQUIRE(info.bytes_in_file() < max_file_size + 2 * record_bytes + 1000000);
      if (mode == "one-event-per-file") {
        BOOST_REQUIRE(info.bytes_in_file() < 2 * record_bytes + 1000000);
      }
      run_new_bytes += info.new_bytes_output();
    }
    data_store_ptr->finish_with_run(run_number);

    // all of this run's bytes, and only those, were counted: no more than its files hold on disk,
    // allowing a little per file because HDF5 may release unused space when a file is closed
    uint64_t bytes_on_disk = 0; // NOLINT(build/unsigned)
    std::string run_pattern = "hdf5writetest_raw_run0000" + std::to_string(run_number) + "_.*\\.hdf5";
    auto run_files = get_files_matching_pattern(file_path, run_pattern);
    BOOST_REQUIRE(!run_files.empty());
    for (auto const& file : run_files) {
      bytes_on_disk += std::filesystem::file_size(file);
    }
    BOOST_TEST_INFO("mode " << mode << ", run " << run_number);
    BOOST_REQUIRE(run_new_bytes >= records_per_run * record_bytes);
    BOOST_REQUIRE(run_new_bytes <= bytes_on_disk + 65536 * run_files.size());
  }

  data_store_ptr.reset();
  delete_files_matching_pattern(file_path, delete_pattern);
}
} // namespace

BOOST_AUTO_TEST_CASE(OpMonCountersOneEventPerFile)
{
  check_opmon_counters_over_runs_and_rollovers("one-event-per-file", 4294967296, 3);
}

BOOST_AUTO_TEST_CASE(OpMonCountersFileSizeLimit)
{
  // about two records per file
  check_opmon_counters_over_runs_and_rollovers("all-per-file", 700000, 6);
}

BOOST_AUTO_TEST_SUITE_END()
