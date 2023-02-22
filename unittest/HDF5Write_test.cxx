/**
 * @file HDF5Write_test.cxx Application that tests and demonstrates
 * the write functionality of the HDF5DataStore class.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/DataStore.hpp"
#include "dfmodules/hdf5datastore/Nljs.hpp"
#include "dfmodules/hdf5datastore/Structs.hpp"

#include "detdataformats/DetID.hpp"
#include "detchannelmaps/HardwareMapService.hpp"
#include "hdf5libs/hdf5filelayout/Nljs.hpp"
#include "hdf5libs/hdf5filelayout/Structs.hpp"

#define BOOST_TEST_MODULE HDF5Write_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

dunedaq::hdf5libs::hdf5filelayout::FileLayoutParams
create_file_layout_params()
{
  dunedaq::hdf5libs::hdf5filelayout::PathParams params1;
  params1.detector_group_type = "Detector_Readout";
  params1.detector_group_name = "TPC";
  params1.element_name_prefix = "Link";
  params1.digits_for_element_number = 10;

  dunedaq::hdf5libs::hdf5filelayout::PathParamList param_list;
  param_list.push_back(params1);

  dunedaq::hdf5libs::hdf5filelayout::FileLayoutParams layout_params;
  layout_params.digits_for_record_number = 5;
  layout_params.path_param_list = param_list;

  return layout_params;
}

dunedaq::daqdataformats::TriggerRecord
create_trigger_record(int trig_num, int fragment_size, int element_count)
{
  const int run_number = 53;
  const dunedaq::daqdataformats::SourceID::Subsystem stype_to_use =
    dunedaq::daqdataformats::SourceID::Subsystem::kDetectorReadout;

  // setup our dummy_data
  std::vector<char> dummy_vector(fragment_size);
  char* dummy_data = dummy_vector.data();

  // get a timestamp for this trigger
  uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>( // NOLINT(build/unsigned)
                  system_clock::now().time_since_epoch())
                  .count();

  // create TriggerRecordHeader
  dunedaq::daqdataformats::TriggerRecordHeaderData trh_data;
  trh_data.trigger_number = trig_num;
  trh_data.trigger_timestamp = ts;
  trh_data.num_requested_components = element_count;
  trh_data.run_number = run_number;
  trh_data.sequence_number = 0;
  trh_data.max_sequence_number = 1;
  trh_data.element_id = dunedaq::daqdataformats::SourceID(dunedaq::daqdataformats::SourceID::Subsystem::kTRBuilder, 0);

  dunedaq::daqdataformats::TriggerRecordHeader trh(&trh_data);

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

dunedaq::detchannelmaps::HardwareMap
make_hardware_map(std::string file_path, int app_count, int link_count, int det_id = 3)
{
  dunedaq::detchannelmaps::HardwareMap output;
  int sid = 0;

  for (int app = 0; app < app_count; ++app) {
    for (int link = 0; link < link_count; ++link) {
      dunedaq::detchannelmaps::HWInfo info;
      info.dro_source_id = sid;
      info.det_link = sid % 2;
      info.det_slot = sid / 2;
      info.det_crate = app;
      info.det_id = det_id;
      info.dro_host = "localhost";
      info.dro_card = app;
      info.dro_slr = link / 5;
      info.dro_link = link % 5;
      info.from_file = true;
      output.link_infos.push_back(info);
      ++sid;
    }
  }
  return output;
}

BOOST_AUTO_TEST_SUITE(HDF5Write_test)

BOOST_AUTO_TEST_CASE(WriteEventFiles)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid()) + "_" + std::string(getenv("USER"));

  const int trigger_count = 5;
  const int apa_count = 3;
  const int link_count = 1;
  const int fragment_size = 10 + sizeof(dunedaq::daqdataformats::FragmentHeader);

  // Make a hardware map
  auto hardware_map = make_hardware_map(file_path, apa_count, link_count);

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*\\.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  hdf5datastore::ConfParams config_params;
  config_params.name = "tempWriter";
  config_params.directory_path = file_path;
  config_params.mode = "one-event-per-file";
  config_params.filename_parameters.overall_prefix = file_prefix;
  config_params.filename_parameters.writer_identifier = "HDF5Write_test";
  config_params.file_layout_parameters = create_file_layout_params();
  config_params.hardware_map = hardware_map;

  hdf5datastore::data_t hdf5ds_json;
  hdf5datastore::to_json(hdf5ds_json, config_params);

  std::unique_ptr<DataStore> data_store_ptr;
  data_store_ptr = make_data_store(hdf5ds_json);

  // write several events, each with several fragments
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number)
    data_store_ptr->write(create_trigger_record(trigger_number, fragment_size, link_count * apa_count));

  data_store_ptr.reset(); // explicit destruction

  // check that the expected number of files was created
  std::string search_pattern = file_prefix + ".*\\.hdf5";
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
  std::string file_prefix = "demo" + std::to_string(getpid()) + "_" + std::string(getenv("USER"));

  const int trigger_count = 5;
  const int apa_count = 3;
  const int link_count = 1;
  const int fragment_size = 10 + sizeof(dunedaq::daqdataformats::FragmentHeader);

  // Make a hardware map
  auto hardware_map = make_hardware_map(file_path, apa_count, link_count);

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*\\.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  hdf5datastore::ConfParams config_params;
  config_params.name = "tempWriter";
  config_params.directory_path = file_path;
  config_params.mode = "all-per-file";
  config_params.max_file_size_bytes = 100000000; // much larger than what we expect, so no second file;
  config_params.filename_parameters.overall_prefix = file_prefix;
  config_params.filename_parameters.writer_identifier = "HDF5Write_test";
  config_params.file_layout_parameters = create_file_layout_params();
  config_params.hardware_map = hardware_map;

  hdf5datastore::data_t hdf5ds_json;
  hdf5datastore::to_json(hdf5ds_json, config_params);

  std::unique_ptr<DataStore> data_store_ptr;
  data_store_ptr = make_data_store(hdf5ds_json);

  // write several events, each with several fragments
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number)
    data_store_ptr->write(create_trigger_record(trigger_number, fragment_size, apa_count * link_count));

  data_store_ptr.reset(); // explicit destruction

  // check that the expected number of files was created
  std::string search_pattern = file_prefix + ".*\\.hdf5";
  std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);

  // clean up the files that were created
  file_list = delete_files_matching_pattern(file_path, delete_pattern);
  delete_files_matching_pattern(file_path, "HardwareMap.*\\.txt");
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);
}

BOOST_AUTO_TEST_CASE(CheckWritingSuffix)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid()) + "_" + std::string(getenv("USER"));

  const int trigger_count = 5;
  const int apa_count = 3;
  const int link_count = 1;
  const int fragment_size = 10 + sizeof(dunedaq::daqdataformats::FragmentHeader);

  // Make a hardware map
  auto hardware_map = make_hardware_map(file_path, apa_count, link_count);

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*\\.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  hdf5datastore::ConfParams config_params;
  config_params.name = "tempWriter";
  config_params.directory_path = file_path;
  config_params.mode = "all-per-file";
  config_params.max_file_size_bytes = 100000000; // much larger than what we expect, so no second file;
  config_params.filename_parameters.overall_prefix = file_prefix;
  config_params.filename_parameters.writer_identifier = "HDF5Write_test";
  config_params.file_layout_parameters = create_file_layout_params();
  config_params.hardware_map = hardware_map;

  hdf5datastore::data_t hdf5ds_json;
  hdf5datastore::to_json(hdf5ds_json, config_params);

  std::unique_ptr<DataStore> data_store_ptr;
  data_store_ptr = make_data_store(hdf5ds_json);

  // write several events, each with several fragments
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number) {
    data_store_ptr->write(create_trigger_record(trigger_number, fragment_size, apa_count * link_count));

    // check that the .writing file is there
    std::string search_pattern = file_prefix + ".*\\.writing";
    std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
    BOOST_REQUIRE_EQUAL(file_list.size(), 1);
  }

  data_store_ptr.reset(); // explicit destruction

  // check that the expected number of files was created
  std::string search_pattern = file_prefix + ".*\\.writing";
  std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
  BOOST_REQUIRE_EQUAL(file_list.size(), 0);

  // clean up the files that were created
  file_list = delete_files_matching_pattern(file_path, delete_pattern);
  delete_files_matching_pattern(file_path, "HardwareMap.*\\.txt");
  BOOST_REQUIRE_EQUAL(file_list.size(), 1);
}

BOOST_AUTO_TEST_CASE(FileSizeLimitResultsInMultipleFiles)
{
  std::string file_path(std::filesystem::temp_directory_path());
  std::string file_prefix = "demo" + std::to_string(getpid()) + "_" + std::string(getenv("USER"));

  const int trigger_count = 15;
  const int apa_count = 5;
  const int link_count = 10;
  const int fragment_size = 10000;

  // Make a hardware map
  auto hardware_map = make_hardware_map(file_path, apa_count, link_count);

  // 5 APAs times 10 links times 10000 bytes per fragment gives 500,000 bytes per TR
  // So, 15 TRs would give 7,500,000 bytes total.

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*\\.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  hdf5datastore::ConfParams config_params;
  config_params.name = "tempWriter";
  config_params.directory_path = file_path;
  config_params.mode = "all-per-file";
  config_params.max_file_size_bytes = 3000000; // goal is 6 events per file
  config_params.filename_parameters.overall_prefix = file_prefix;
  config_params.filename_parameters.writer_identifier = "HDF5Write_test";
  config_params.file_layout_parameters = create_file_layout_params();
  config_params.hardware_map = hardware_map;

  hdf5datastore::data_t hdf5ds_json;
  hdf5datastore::to_json(hdf5ds_json, config_params);

  std::unique_ptr<DataStore> data_store_ptr;
  data_store_ptr = make_data_store(hdf5ds_json);

  // write several events, each with several fragments
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number)
    data_store_ptr->write(create_trigger_record(trigger_number, fragment_size, apa_count * link_count));

  data_store_ptr.reset(); // explicit destruction

  // check that the expected number of files was created
  std::string search_pattern = file_prefix + ".*\\.hdf5";
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
  std::string file_prefix = "demo" + std::to_string(getpid()) + "_" + std::string(getenv("USER"));

  const int trigger_count = 5;
  const int apa_count = 5;
  const int link_count = 1;
  const int fragment_size = 100000;

  // Make a hardware map
  auto hardware_map = make_hardware_map(file_path, apa_count, link_count);

  // 5 APAs times 100000 bytes per fragment gives 500,000 bytes per TR

  // delete any pre-existing files so that we start with a clean slate
  std::string delete_pattern = file_prefix + ".*\\.hdf5";
  delete_files_matching_pattern(file_path, delete_pattern);

  // create the DataStore
  hdf5datastore::ConfParams config_params;
  config_params.name = "tempWriter";
  config_params.directory_path = file_path;
  config_params.mode = "all-per-file";
  config_params.max_file_size_bytes = 150000; // ~1.5 Fragment, ~0.3 TR
  config_params.filename_parameters.overall_prefix = file_prefix;
  config_params.filename_parameters.writer_identifier = "HDF5Write_test";
  config_params.file_layout_parameters = create_file_layout_params();
  config_params.hardware_map = hardware_map;

  hdf5datastore::data_t hdf5ds_json;
  hdf5datastore::to_json(hdf5ds_json, config_params);

  std::unique_ptr<DataStore> data_store_ptr;
  data_store_ptr = make_data_store(hdf5ds_json);

  // write several events, each with several fragments
  for (int trigger_number = 1; trigger_number <= trigger_count; ++trigger_number)
    data_store_ptr->write(create_trigger_record(trigger_number, fragment_size, apa_count * link_count));

  data_store_ptr.reset(); // explicit destruction

  // check that the expected number of files was created
  std::string search_pattern = file_prefix + ".*\\.hdf5";
  std::vector<std::string> file_list = get_files_matching_pattern(file_path, search_pattern);
  // each TriggerRecord should be stored in its own file
  BOOST_REQUIRE_EQUAL(file_list.size(), 5);

  // clean up the files that were created
  file_list = delete_files_matching_pattern(file_path, delete_pattern);
  delete_files_matching_pattern(file_path, "HardwareMap.*\\.txt");
  BOOST_REQUIRE_EQUAL(file_list.size(), 5);
}

BOOST_AUTO_TEST_SUITE_END()
