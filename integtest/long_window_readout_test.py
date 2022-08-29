import pytest
import os
import re
import copy

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.config_file_gen as config_file_gen
import dfmodules.integtest_file_gen as integtest_file_gen

# Values that help determine the running conditions
number_of_data_producers=2
run_duration=22  # seconds
number_of_readout_apps=3
number_of_dataflow_apps=1
trigger_rate=0.1 # Hz
token_count=1
readout_window_time_before=37200000  # 0.764 second is the intention for b+a
readout_window_time_after=1000000
trigger_record_max_window=200000     # intention is 4 msec
clock_speed_hz=50000000
latency_buffer_size=600000
data_rate_slowdown_factor=20

# Default values for validation parameters
expected_number_of_data_files=2*number_of_dataflow_apps
check_for_logfile_errors=True
expected_event_count=191 # 3*run_duration*trigger_rate/number_of_dataflow_apps
expected_event_count_tolerance=5
wib1_frag_hsi_trig_params={"fragment_type_description": "WIB", 
                           "fragment_type": "ProtoWIB",
                           "hdf5_source_subsystem": "Detector_Readout",
                           "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                           "min_size_bytes": 3712080, "max_size_bytes": 3712080}
triggercandidate_frag_params={"fragment_type_description": "Trigger Candidate",
                              "fragment_type": "Trigger_Candidate",
                              "hdf5_source_subsystem": "Trigger",
                              "expected_fragment_count": 1,
                              "min_size_bytes": 80, "max_size_bytes": 150}

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="daqconf_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
hardware_map_contents = integtest_file_gen.generate_hwmap_file(number_of_data_producers, number_of_readout_apps)

conf_dict = config_file_gen.get_default_config_dict()
conf_dict["readout"]["data_rate_slowdown_factor"] = data_rate_slowdown_factor
conf_dict["readout"]["clock_speed_hz"] = clock_speed_hz
conf_dict["readout"]["latency_buffer_size"] = latency_buffer_size
conf_dict["trigger"]["trigger_rate_hz"] = trigger_rate
conf_dict["trigger"]["trigger_window_before_ticks"] = readout_window_time_before
conf_dict["trigger"]["trigger_window_after_ticks"] = readout_window_time_after

for df_app in range(1, number_of_dataflow_apps):
    dfapp_key = f"dataflow.dataflow{df_app}"
    conf_dict[dfapp_key] = {}

trsplit_conf = copy.deepcopy(conf_dict)
for df_app in range(number_of_dataflow_apps):
    dfapp_key = f"dataflow.dataflow{df_app}"
    trsplit_conf[dfapp_key]["max_trigger_record_window"] = trigger_record_max_window

confgen_arguments={#"No_TR_Splitting": conf_dict,
                   "With_TR_Splitting": trsplit_conf,
                  }

# The commands to run in nanorc, as a list
nanorc_command_list="integtest-partition boot conf".split()
nanorc_command_list+="start_run --wait 15 101 wait ".split() + [str(run_duration)] + "stop_run --wait 2 wait 2".split()
nanorc_command_list+="start 102 wait 15 enable_triggers wait ".split() + [str(run_duration)] + "stop_run wait 2".split()
nanorc_command_list+="scrap terminate".split()

# The tests themselves

def test_nanorc_success(run_nanorc):
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
    if check_for_logfile_errors:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(run_nanorc.log_files, True, True)

def test_data_files(run_nanorc):
    local_expected_event_count=expected_event_count
    local_event_count_tolerance=expected_event_count_tolerance
    fragment_check_list=[]
    fragment_check_list.append(wib1_frag_hsi_trig_params)
    fragment_check_list.append(triggercandidate_frag_params)

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
    print("============================================")
    print("Listing the hdf5 files before deleting them:")
    print("============================================")
    pathlist_string=""
    filelist_string=""
    for data_file in run_nanorc.data_files:
        filelist_string += " " + str(data_file)
        if str(data_file.parent) not in pathlist_string:
            pathlist_string += " " + str(data_file.parent)

    os.system(f"df -h {pathlist_string}")
    print("--------------------")
    os.system(f"ls -alF {filelist_string}");

    for data_file in run_nanorc.data_files:
        data_file.unlink()

    print("--------------------")
    os.system(f"df -h {pathlist_string}")
    print("============================================")
