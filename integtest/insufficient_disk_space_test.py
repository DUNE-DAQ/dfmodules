import pytest
import os
import re
import shutil

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks

# 21-Jul-2022, KAB: 
# --> changes that are needed in this script include the following:
# * add intelligence to verify that the output disk is small enough
#
# --> problems in the C++ code that this script currently highlights
# * the crash of the DF App in the second run
# * the need for the HDF5DataStore to stop retrying writes at stop (drain-dataflow?) time

# check how much free space there is on the configured output disk
output_path="."
#disk_space=shutil.disk_usage(output_path)
#print(disk_space.total / (1024*1024*1024))
#print(tmpdir)

# Values that help determine the running conditions
number_of_data_producers=10
run_duration=23  # seconds
number_of_readout_apps=3
number_of_dataflow_apps=1
trigger_rate=0.2 # Hz
token_count=1
readout_window_time_before=9000000
readout_window_time_after=1000000

# Default values for validation parameters
expected_number_of_data_files=3
check_for_logfile_errors=True
expected_event_count=4
expected_event_count_tolerance=1
wib1_frag_hsi_trig_params={"fragment_type_description": "WIB",
                           "hdf5_detector_group": "TPC", "hdf5_region_prefix": "APA",
                           "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                           "min_size_bytes": 3712080, "max_size_bytes": 3712080}
triggercandidate_frag_params={"fragment_type_description": "Trigger Candidate",
                              "hdf5_detector_group": "Trigger", "hdf5_region_prefix": "Region",
                              "expected_fragment_count": 1,
                              "min_size_bytes": 72, "max_size_bytes": 216}
hsi_frag_params ={"fragment_type_description": "HSI",
                             "fragment_type": "Hardware_Signal",
                             "hdf5_source_subsystem": "HW_Signals_Interface",
                             "expected_fragment_count": 1,
                             "min_size_bytes": 72, "max_size_bytes": 96}
# TODO, Eric Flumerfelt <eflumerf@github.com> Sep-02-2022: Remove HSI exception once empty fragment issue is fixed
ignored_logfile_problems={"dataflow": ["A problem was encountered when writing TriggerRecord number",
                                       "A problem was encountered when writing a trigger record to file",
                                       r"There are \d+ bytes free, and the required minimum is \d+ bytes based on a safety factor of 5 times the trigger record size"],
                            "hsi": ["Trigger Matching result with empty fragment", "Request on empty buffer: Data not found"]
                         }

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="daqconf_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
confgen_arguments_base=["-o", output_path, "-s", "10", "-n", str(number_of_data_producers), "-b", str(readout_window_time_before), "-a", str(readout_window_time_after), "-t", str(trigger_rate), "--use-fake-data-producers", "--clock-speed-hz", "50000000"] + [ "--host-ru", "localhost" ] * number_of_readout_apps + [ "--host-df", "localhost" ] * number_of_dataflow_apps
confgen_arguments={"Base_System": confgen_arguments_base,
                  }
# The commands to run in nanorc, as a list
nanorc_command_list="integtest-partition boot conf".split()
nanorc_command_list+="start_run 101 wait ".split() + [str(run_duration)] + "stop_run --wait 2 wait 2".split()
nanorc_command_list+="start 102 wait 3 enable_triggers wait ".split() + [str(run_duration)] + "stop_run wait 2".split()
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
    fragment_check_list.append(wib1_frag_hsi_trig_params)

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
