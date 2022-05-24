import pytest
import os
import re

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks

# Values that help determine the running conditions
number_of_data_producers=2
run_duration=62  # seconds
number_of_readout_apps=1
number_of_dataflow_apps=1
trigger_rate=0.2 # Hz

# Default values for validation parameters
expected_number_of_data_files=3*number_of_dataflow_apps
check_for_logfile_errors=True
expected_event_count=3*run_duration*trigger_rate/number_of_dataflow_apps
expected_event_count_tolerance=6
wib1_frag_hsi_trig_params={"fragment_type_description": "WIB",
                           "hdf5_detector_group": "TPC", "hdf5_region_prefix": "APA",
                           "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                           "min_size_bytes": 240000, "max_size_bytes": 251000}

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="daqconf_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
confgen_arguments_base=[ "-d", "./frames.bin", "-o", ".", "-s", "10", "-n", str(number_of_data_producers), "-b", "20000", "-a", "20000", "-t", str(trigger_rate), "--latency-buffer-size", "200000"] + [ "--host-ru", "localhost" ] * number_of_readout_apps + [ "--host-df", "localhost" ] * number_of_dataflow_apps
for idx in range(number_of_readout_apps):
    confgen_arguments_base+=["--region-id", str(idx)] 
confgen_arguments={#"No_TR_Splitting": confgen_arguments_base,
                   "With_TR_Splitting": confgen_arguments_base+["--max-trigger-record-window", "13500"],
                  }
# The commands to run in nanorc, as a list
nanorc_command_list="boot partition-test init conf".split()
nanorc_command_list+="start                 101 wait ".split() + [str(run_duration)] + "stop --stop-wait 2 wait 2".split()
nanorc_command_list+="start --resume-wait 1 102 wait ".split() + [str(run_duration)] + "stop               wait 2".split()
nanorc_command_list+="start --resume-wait 2 103 wait ".split() + [str(run_duration)] + "stop --stop-wait 1 wait 2".split()
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
        assert log_file_checks.logs_are_error_free(run_nanorc.log_files)

def test_data_file(run_nanorc):
    local_expected_event_count=expected_event_count
    local_event_count_tolerance=expected_event_count_tolerance
    fragment_check_list=[]
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
