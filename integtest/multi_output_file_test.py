import pytest
import os
import re

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks

# Values that help determine the running conditions
number_of_data_producers=2
number_of_readout_apps=3

# Default values for validation parameters
expected_number_of_data_files=4
check_for_logfile_errors=True
wib1_frag_hsi_trig_params={"fragment_type_description": "WIB",
                           "hdf5_detector_group": "TPC", "hdf5_region_prefix": "APA",
                           "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                           "min_size_bytes": 37200, "max_size_bytes": 185680}
wib1_frag_multi_trig_params={"fragment_type_description": "WIB",
                             "hdf5_detector_group": "TPC", "hdf5_region_prefix": "APA",
                             "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                             "min_size_bytes": 80, "max_size_bytes": 185680}
triggercandidate_frag_params={"fragment_type_description": "Trigger Candidate",
                              "hdf5_detector_group": "Trigger", "hdf5_region_prefix": "Region",
                              "expected_fragment_count": 1,
                              "min_size_bytes": 130, "max_size_bytes": 150}
triggertp_frag_params={"fragment_type_description": "Trigger with TPs",
                       "hdf5_detector_group": "Trigger", "hdf5_region_prefix": "Region",
                       "expected_fragment_count": ((number_of_data_producers*number_of_readout_apps)+number_of_readout_apps+1),
                       "min_size_bytes": 80, "max_size_bytes": 16000}
ignored_logfile_problems={"trigger": ["zipped_tpset_q: Unable to push within timeout period"]}

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="daqconf_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
confgen_arguments_base=[ "-d", "./frames.bin", "-o", ".", "-s", "10", "-n", str(number_of_data_producers), "-b", "5000", "-a", "5000", "-t", "10.0", "--max-file-size", "1074000000", "--latency-buffer-size", "200000"] + [ "--host-ru", "localhost" ] * number_of_readout_apps
for idx in range(number_of_readout_apps):
    confgen_arguments_base+=["--region-id", str(idx)] 
confgen_arguments={"WIB1_System": confgen_arguments_base,
                   "Software_TPG_System": confgen_arguments_base+["--enable-software-tpg", "-c", str(3*number_of_data_producers*number_of_readout_apps)]}
# The commands to run in nanorc, as a list
nanorc_command_list="boot test-partition init conf start 101 resume wait 180 pause wait 2 stop wait 21 start 102 resume wait 120 pause wait 2 stop wait 21 scrap terminate".split()

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

def test_data_file(run_nanorc):
    local_file_count=expected_number_of_data_files
    fragment_check_list=[]
    if "--enable-software-tpg" in run_nanorc.confgen_arguments:
        local_file_count=5
        fragment_check_list.append(wib1_frag_multi_trig_params)
        fragment_check_list.append(triggertp_frag_params)
    else:
        fragment_check_list.append(wib1_frag_hsi_trig_params)
        fragment_check_list.append(triggercandidate_frag_params)

    # Run some tests on the output data file
    assert len(run_nanorc.data_files)==local_file_count

    for idx in range(len(run_nanorc.data_files)):
        data_file=data_file_checks.DataFile(run_nanorc.data_files[idx])
        assert data_file_checks.sanity_check(data_file)
        assert data_file_checks.check_file_attributes(data_file)
        for jdx in range(len(fragment_check_list)):
            assert data_file_checks.check_fragment_count(data_file, fragment_check_list[jdx])
            assert data_file_checks.check_fragment_sizes(data_file, fragment_check_list[jdx])
