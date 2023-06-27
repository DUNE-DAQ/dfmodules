import pytest
import os
import re
import copy
import shutil
import psutil
import urllib.request

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.config_file_gen as config_file_gen
import dfmodules.integtest_file_gen as integtest_file_gen

# Values that help determine the running conditions
output_path_parameter="."
number_of_data_producers=4
run_duration=40  # seconds
number_of_readout_apps=3
number_of_dataflow_apps=1
trigger_rate=0.05 # Hz
token_count=1
readout_window_time_before=100000000  # 1.616 second is the intention for b+a
readout_window_time_after=1000000
trigger_record_max_window=500000     # intention is 8 msec
latency_buffer_size=600000
data_rate_slowdown_factor=1
minimum_cpu_count=24
minimum_free_memory_gb=52
minimum_total_disk_space_gb=32  # double what we need
minimum_free_disk_space_gb=24   # 50% more than what we need

# Default values for validation parameters
expected_number_of_data_files=4*number_of_dataflow_apps
check_for_logfile_errors=True
expected_event_count=202
expected_event_count_tolerance= 9
wib1_frag_hsi_trig_params={"fragment_type_description": "WIB",
                           "fragment_type": "ProtoWIB",
                           "hdf5_source_subsystem": "Detector_Readout",
                           "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                           "min_size_bytes": 3712072, "max_size_bytes": 3712536}
wib2_frag_params={"fragment_type_description": "WIB2",
                  "fragment_type": "WIB",
                  "hdf5_source_subsystem": "Detector_Readout",
                  "expected_fragment_count": number_of_data_producers*number_of_readout_apps,
                  "min_size_bytes": 29808, "max_size_bytes": 30280}
wibeth_frag_params={"fragment_type_description": "WIBEth",
                  "fragment_type": "WIBEth",
                  "hdf5_source_subsystem": "Detector_Readout",
                  "expected_fragment_count": number_of_data_producers*number_of_readout_apps,
                  "min_size_bytes": 1764072, "max_size_bytes": 1771272}
triggercandidate_frag_params={"fragment_type_description": "Trigger Candidate",
                              "fragment_type": "Trigger_Candidate",
                              "hdf5_source_subsystem": "Trigger",
                              "expected_fragment_count": 1,
                              "min_size_bytes": 72, "max_size_bytes": 216}
hsi_frag_params ={"fragment_type_description": "HSI",
                             "fragment_type": "Hardware_Signal",
                             "hdf5_source_subsystem": "HW_Signals_Interface",
                             "expected_fragment_count": 1,
                             "min_size_bytes": 72, "max_size_bytes": 100}
ignored_logfile_problems={}

# Determine if the conditions are right for these tests
sufficient_disk_space=True
actual_output_path=output_path_parameter
if output_path_parameter == ".":
    actual_output_path="/tmp"
disk_space=shutil.disk_usage(actual_output_path)
total_disk_space_gb=(disk_space.total/(1024*1024*1024))
free_disk_space_gb=(disk_space.free/(1024*1024*1024))
print(f"DEBUG: Space on disk for output path {actual_output_path}: total = {total_disk_space_gb} GB and free = {free_disk_space_gb} GB.")
if total_disk_space_gb < minimum_total_disk_space_gb or free_disk_space_gb < minimum_free_disk_space_gb:
    sufficient_disk_space=False
sufficient_resources_on_this_computer=True
cpu_count=os.cpu_count()
hostname=os.uname().nodename
mem_obj=psutil.virtual_memory()
free_mem=round((mem_obj.available/(1024*1024*1024)), 2)
total_mem=round((mem_obj.total/(1024*1024*1024)), 2)
print(f"DEBUG: CPU count is {cpu_count}, free and total memory are {free_mem} GB and {total_mem} GB.")
if cpu_count < minimum_cpu_count or free_mem < minimum_free_memory_gb:
    sufficient_resources_on_this_computer=False

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="daqconf_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
dro_map_contents = integtest_file_gen.generate_dromap_contents(number_of_data_producers, number_of_readout_apps)

conf_dict = config_file_gen.get_default_config_dict()
conf_dict["daq_common"]["data_rate_slowdown_factor"] = data_rate_slowdown_factor
conf_dict["readout"]["latency_buffer_size"] = latency_buffer_size
#conf_dict["readout"]["default_data_file"] = "asset://?label=DuneWIB&subsystem=readout" # DuneWIB
conf_dict["readout"]["default_data_file"] = "asset://?checksum=e96fd6efd3f98a9a3bfaba32975b476e" # WIBEth
conf_dict["detector"]["clock_speed_hz"] = 62500000 # DuneWIB/WIBEth
conf_dict["readout"]["use_fake_cards"] = True
conf_dict["readout"]["emulated_data_times_start_with_now"] = True
conf_dict["hsi"]["random_trigger_rate_hz"] = trigger_rate
conf_dict["trigger"]["trigger_window_before_ticks"] = readout_window_time_before
conf_dict["trigger"]["trigger_window_after_ticks"] = readout_window_time_after

conf_dict["dataflow"]["token_count"] = token_count
conf_dict["dataflow"]["apps"] = [] # Remove preconfigured dataflow0 app
for df_app in range(number_of_dataflow_apps):
    dfapp_conf = {}
    dfapp_conf["app_name"] = f"dataflow{df_app}"
    dfapp_conf["max_file_size"] = 4*1024*1024*1024
    dfapp_conf["output_paths"] = [ output_path_parameter ]
    conf_dict["dataflow"]["apps"].append(dfapp_conf)

trsplit_conf = copy.deepcopy(conf_dict)
for df_app in range(number_of_dataflow_apps):
    trsplit_conf["dataflow"]["apps"][df_app]["max_trigger_record_window"] = trigger_record_max_window

confgen_arguments={#"No_TR_Splitting": conf_dict,
                   "With_TR_Splitting": trsplit_conf,
                  }

# The commands to run in nanorc, as a list
if sufficient_disk_space and sufficient_resources_on_this_computer:
    nanorc_command_list="integtest-partition boot conf".split()
    nanorc_command_list+="start_run --wait 15 101 wait ".split() + [str(run_duration)] + "stop_run --wait 2 wait 2".split()
    nanorc_command_list+="start 102 wait 15 enable_triggers wait ".split() + [str(run_duration)] + "stop_run wait 2".split()
    nanorc_command_list+="scrap terminate".split()
else:
    nanorc_command_list=["integtest-partition", "boot", "terminate"]

# The tests themselves

def test_nanorc_success(run_nanorc):
    if not sufficient_resources_on_this_computer:
        pytest.skip(f"This computer ({hostname}) does not have enough resources to run this test.")
    if not sufficient_disk_space:
        pytest.skip(f"The raw data output path ({actual_output_path}) does not have enough space to run this test.")

    current_test=os.environ.get('PYTEST_CURRENT_TEST')
    match_obj = re.search(r".*\[(.+)\].*", current_test)
    if match_obj:
        current_test = match_obj.group(1)
    banner_line = re.sub(".", "=", current_test)
    print(banner_line)
    print(current_test)
    print(banner_line)
    # Check that nanorc completed correctly
    assert run_nanorc.completed_process.returncode==0

def test_log_files(run_nanorc):
    if not sufficient_resources_on_this_computer:
        pytest.skip(f"This computer ({hostname}) does not have enough resources to run this test.")
    if not sufficient_disk_space:
        pytest.skip(f"The raw data output path ({actual_output_path}) does not have enough space to run this test.")

    if check_for_logfile_errors:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(run_nanorc.log_files, True, True, ignored_logfile_problems)

def test_data_files(run_nanorc):
    if not sufficient_resources_on_this_computer:
        print(f"This computer ({hostname}) does not have enough resources to run this test.")
        print(f"    (CPU count is {cpu_count}, free and total memory are {free_mem} GB and {total_mem} GB.)")
        print(f"    (Minimum CPU count is {minimum_cpu_count} and minimum free memory is {minimum_free_memory_gb} GB.)")
        pytest.skip(f"This computer ({hostname}) does not have enough resources to run this test.")

    if not sufficient_disk_space:
        print(f"The raw data output path ({actual_output_path}) does not have enough space to run this test.")
        print(f"    (Free and total space are {free_disk_space_gb} GB and {total_disk_space_gb} GB.)")
        print(f"    (Minimums are {minimum_free_disk_space_gb} GB and {minimum_total_disk_space_gb} GB.)")
        pytest.skip(f"The raw data output path ({actual_output_path}) does not have enough space to run this test.")

    local_expected_event_count=expected_event_count
    local_event_count_tolerance=expected_event_count_tolerance
    fragment_check_list=[triggercandidate_frag_params, hsi_frag_params]
    #fragment_check_list.append(wib1_frag_hsi_trig_params) # ProtoWIB
    #fragment_check_list.append(wib2_frag_params) # DuneWIB
    fragment_check_list.append(wibeth_frag_params) # WIBEth

    # Run some tests on the output data file
    assert len(run_nanorc.data_files)==expected_number_of_data_files

    for idx in range(len(run_nanorc.data_files)):
        data_file=data_file_checks.DataFile(run_nanorc.data_files[idx])
        assert data_file_checks.sanity_check(data_file)
        assert data_file_checks.check_file_attributes(data_file)
        assert data_file_checks.check_event_count(data_file, local_expected_event_count, local_event_count_tolerance)
        for jdx in range(len(fragment_check_list)):
            assert data_file_checks.check_fragment_count(data_file, fragment_check_list[jdx])
            assert data_file_checks.check_fragment_sizes(data_file, fragment_check_list[jdx])

def test_cleanup(run_nanorc):
    if not sufficient_disk_space:
        pytest.skip(f"The raw data output path ({actual_output_path}) does not have enough space to run this test.")

    pathlist_string=""
    filelist_string=""
    for data_file in run_nanorc.data_files:
        filelist_string += " " + str(data_file)
        if str(data_file.parent) not in pathlist_string:
            pathlist_string += " " + str(data_file.parent)

    if pathlist_string and filelist_string:
        print("============================================")
        print("Listing the hdf5 files before deleting them:")
        print("============================================")

        os.system(f"df -h {pathlist_string}")
        print("--------------------")
        os.system(f"ls -alF {filelist_string}");

        for data_file in run_nanorc.data_files:
            data_file.unlink()

        print("--------------------")
        os.system(f"df -h {pathlist_string}")
        print("============================================")
