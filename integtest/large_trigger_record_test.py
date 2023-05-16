# two of the goals of this test are to verify that...
# * if the TR size is slightly over half of the maximum HDF5 file size, then
#   we see one TR per file
# * if the TR size is larger than the maximum HDF5 file size, we also see one
#   TR per file.

import pytest
import os
import re
import urllib.request
import copy

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.config_file_gen as config_file_gen
import dfmodules.integtest_file_gen as integtest_file_gen

# Values that help determine the running conditions
number_of_data_producers=10
run_duration=32  # seconds
number_of_readout_apps=3
number_of_dataflow_apps=1
trigger_rate=0.06 # Hz
token_count=1
readout_window_time_before=10000000
readout_window_time_after=1000000
data_rate_slowdown_factor=1

# Default values for validation parameters
expected_number_of_data_files=4
check_for_logfile_errors=True
expected_event_count=1
expected_event_count_tolerance=1
wibeth_frag_55pct_params={"fragment_type_description": "WIBEth",
                             "fragment_type": "WIBEth",
                             "hdf5_source_subsystem": "Detector_Readout",
                             "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                             "min_size_bytes": 38678472, "max_size_bytes": 38678472}
wibeth_frag_125pct_params={"fragment_type_description": "WIBEth",
                               "fragment_type": "WIBEth",
                               "hdf5_source_subsystem": "Detector_Readout",
                               "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                               "min_size_bytes": 91411272, "max_size_bytes": 91411272}
triggercandidate_frag_params={"fragment_type_description": "Trigger Candidate",
                              "fragment_type": "Trigger_Candidate",
                              "hdf5_source_subsystem": "Trigger",
                              "expected_fragment_count": 1,
                              "min_size_bytes": 72, "max_size_bytes": 280}
hsi_frag_params ={"fragment_type_description": "HSI",
                             "fragment_type": "Hardware_Signal",
                             "hdf5_source_subsystem": "HW_Signals_Interface",
                             "expected_fragment_count": 1,
                             "min_size_bytes": 72, "max_size_bytes": 100}
ignored_logfile_problems={}

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
conf_dict["readout"]["use_fake_data_producers"] = True
conf_dict["readout"]["clock_speed_hz"] = 62500000 # DuneWIB/WIBEth
conf_dict["readout"]["eth_mode"] = True # WIBEth
conf_dict["trigger"]["trigger_rate_hz"] = trigger_rate
conf_dict["trigger"]["trigger_window_before_ticks"] = readout_window_time_before
conf_dict["trigger"]["trigger_window_after_ticks"] = readout_window_time_after

conf_dict["dataflow"]["token_count"] = token_count
conf_dict["dataflow"]["apps"] = [] # Remove preconfigured dataflow0 app
for df_app in range(number_of_dataflow_apps):
    dfapp_conf = {}
    dfapp_conf["app_name"] = f"dataflow{df_app}"
    dfapp_conf["max_file_size"] = 2*1024*1024*1024
    conf_dict["dataflow"]["apps"].append(dfapp_conf)

oversize_conf = copy.deepcopy(conf_dict)
oversize_conf["trigger"]["trigger_window_before_ticks"] = readout_window_time_before * 2.5

confgen_arguments={"TRSize_55PercentOfMaxFileSize": conf_dict,
                   "TRSize_125PercentOfMaxFileSize": oversize_conf,
                  }
# The commands to run in nanorc, as a list
nanorc_command_list="integtest-partition boot conf".split()
nanorc_command_list+="start_run --wait 10 101 wait ".split() + [str(run_duration)] + "stop_run --wait 2 wait 2".split()
nanorc_command_list+="start 102 wait 10 enable_triggers wait ".split() + [str(run_duration)] + "stop_run wait 2".split()
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
        assert log_file_checks.logs_are_error_free(run_nanorc.log_files, True, True, ignored_logfile_problems)

def test_data_files(run_nanorc):
    local_expected_event_count=expected_event_count
    local_event_count_tolerance=expected_event_count_tolerance
    fragment_check_list=[triggercandidate_frag_params, hsi_frag_params]
    if "trigger_window_before_ticks" in run_nanorc.confgen_config["trigger"].keys() and run_nanorc.confgen_config["trigger"]["trigger_window_before_ticks"] > 15000000:
        fragment_check_list.append(wibeth_frag_125pct_params)
    else:
        fragment_check_list.append(wibeth_frag_55pct_params)

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
