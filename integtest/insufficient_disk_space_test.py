import pytest
import os
import re
import shutil
import urllib.request

import integrationtest.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.config_file_gen as config_file_gen
import integrationtest.oks_dro_map_gen as dro_map_gen

pytest_plugins="integrationtest.integrationtest_drunc"

# 21-Jul-2022, KAB:
# --> changes that are needed in this script include the following:
# * add intelligence to verify that the output disk is small enough
#
# --> problems in the C++ code that this script currently highlights
# * the crash of the DF App in the second run
# * the need for the HDF5DataStore to stop retrying writes at stop (drain-dataflow?) time

# 08-May-2023, ELF:
# Tested script by creating a tmpfs ramdisk (as root):
# mkdir /mnt/tmp
# mount -t tmpfs -o size=5g tmpfs /mnt/tmp
# chmod 777 /mnt/tmp
# After testing, umount -lf /mnt/tmp to release memory

# check how much free space there is on the configured output disk
output_path=f"/mnt/tmp"
disk_space=shutil.disk_usage(output_path)
gb_space=disk_space.free / (1024*1024*1024)
gb_limit=6.0
hostname=os.uname().nodename
print(f"DEBUG: {gb_space} GB free in {output_path}")

# Values that help determine the running conditions
number_of_data_producers=10
run_duration=20  # seconds
number_of_readout_apps=3
number_of_dataflow_apps=1
trigger_rate=0.2 # Hz
data_rate_slowdown_factor = 10
token_count=1
readout_window_time_before=9000000
readout_window_time_after=1000000

# Default values for validation parameters
expected_number_of_data_files=1
check_for_logfile_errors=True
expected_event_count=int(gb_space - 5)
expected_event_count_tolerance=1
wib2_frag_hsi_trig_params={"fragment_type_description": "WIB",
                           "fragment_type": "WIB",
                           "hdf5_source_subsystem": "Detector_Readout",
                           "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                           "min_size_bytes": 29808, "max_size_bytes": 30280}
wib2_frag_multi_trig_params={"fragment_type_description": "WIB",
                             "fragment_type": "WIB",
                             "hdf5_source_subsystem": "Detector_Readout",
                             "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                             "min_size_bytes": 72, "max_size_bytes": 54000}
wibeth_frag_hsi_trig_params={"fragment_type_description": "WIBEth",
                  "fragment_type": "WIBEth",
                  "hdf5_source_subsystem": "Detector_Readout",
                  "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                  "min_size_bytes": 35157672, "max_size_bytes": 35157672}
triggercandidate_frag_params={"fragment_type_description": "Trigger Candidate",
                              "fragment_type": "Trigger_Candidate",
                              "hdf5_source_subsystem": "Trigger",
                              "expected_fragment_count": 1,
                              "min_size_bytes": 72, "max_size_bytes": 280}
triggeractivity_frag_params={"fragment_type_description": "Trigger Activity",
                              "fragment_type": "Trigger_Activity",
                              "hdf5_source_subsystem": "Trigger",
                              "expected_fragment_count": number_of_readout_apps,
                              "min_size_bytes": 72, "max_size_bytes": 400}
triggertp_frag_params={"fragment_type_description": "Trigger with TPs",
                       "fragment_type": "Trigger_Primitive",
                       "hdf5_source_subsystem": "Trigger",
                       "expected_fragment_count": ((number_of_data_producers*number_of_readout_apps)),
                       "min_size_bytes": 72, "max_size_bytes": 16000}
hsi_frag_params ={"fragment_type_description": "HSI",
                             "fragment_type": "Hardware_Signal",
                             "hdf5_source_subsystem": "HW_Signals_Interface",
                             "expected_fragment_count": 1,
                             "min_size_bytes": 72, "max_size_bytes": 100}
ignored_logfile_problems={}
required_logfile_problems={"dataflow": ["A problem was encountered when writing TriggerRecord number",
                                       "A problem was encountered when writing a trigger record to file",
                                       r"There are \d+ bytes free, and the required minimum is \d+ bytes based on a safety factor of 5 times the trigger record size"],
                           "trigger": [r"Trigger is inhibited in run \d+"],
                           "dfo": [r"TriggerDecision \d+ didn't complete within timeout in run \d+"]}

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="fddaqconf_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
dro_map_contents = dro_map_gen.generate_dromap_contents(number_of_data_producers, number_of_readout_apps)

conf_dict = config_file_gen.get_default_config_dict()
conf_dict["daq_common"]["data_rate_slowdown_factor"] = data_rate_slowdown_factor
conf_dict["readout"]["use_fake_data_producers"] = True
conf_dict["detector"]["clock_speed_hz"] = 62500000 # DuneWIB/WIBEth
conf_dict["hsi"]["random_trigger_rate_hz"] = trigger_rate
conf_dict["trigger"]["trigger_window_before_ticks"] = readout_window_time_before
conf_dict["trigger"]["trigger_window_after_ticks"] = readout_window_time_after

conf_dict["dataflow"]["token_count"] = token_count
conf_dict["dataflow"]["apps"] = [] # Remove preconfigured dataflow0 app
for df_app in range(number_of_dataflow_apps):
    dfapp_conf = {}
    dfapp_conf["app_name"] = f"dataflow{df_app}"
    dfapp_conf["output_paths"] = [output_path]
    conf_dict["dataflow"]["apps"].append(dfapp_conf)

confgen_arguments={"Base_System": conf_dict,
                  }
# The commands to run in nanorc, as a list
if gb_space < gb_limit:
  nanorc_command_list="boot conf".split()
  nanorc_command_list+="start_run 101 wait ".split() + [str(run_duration)] + "stop_run --wait 2 wait 2".split()
  nanorc_command_list+="start 102 wait 3 enable_triggers wait ".split() + [str(run_duration)] + "stop_run wait 2".split()
  nanorc_command_list+="start 103 wait 3 enable_triggers wait ".split() + [str(run_duration)] + "stop_run wait 2".split()
  nanorc_command_list+="scrap terminate".split()
else:
  nanorc_command_list=["boot", "terminate"]

# The tests themselves

def test_nanorc_success(run_nanorc):
    if gb_space >= gb_limit:
        pytest.skip(f"This computer ({hostname}) has too much available space in {output_path}.\n    (There is {gb_space} GB of space in {output_path}, limit is {gb_limit} GB)")

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
    if gb_space >= gb_limit:
        pytest.skip(f"This computer ({hostname}) has too much available space in {output_path}.\n    (There is {gb_space} GB of space in {output_path}, limit is {gb_limit} GB)")

    if check_for_logfile_errors:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(run_nanorc.log_files, True, True, ignored_logfile_problems, required_logfile_problems)

def test_data_files(run_nanorc):
    if gb_space >= gb_limit:
        print(f"This computer ({hostname}) has too much available space in {output_path}.\n    (There is {gb_space} GB of space in {output_path}, limit is {gb_limit} GB)")
        pytest.skip(f"This computer ({hostname}) has too much available space in {output_path}.\n    (There is {gb_space} GB of space in {output_path}, limit is {gb_limit} GB)")

    local_expected_event_count=expected_event_count
    local_event_count_tolerance=expected_event_count_tolerance
    fragment_check_list=[triggercandidate_frag_params, hsi_frag_params]
    #fragment_check_list.append(wib1_frag_hsi_trig_params)
    #fragment_check_list.append(wib2_frag_hsi_trig_params) # DuneWIB
    fragment_check_list.append(wibeth_frag_hsi_trig_params) # WIBEth

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
    if gb_space >= gb_limit:
        pytest.skip(f"This computer ({hostname}) has too much available space in {output_path}.\n    (There is {gb_space} GB of space in {output_path}, limit is {gb_limit} GB)")

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
