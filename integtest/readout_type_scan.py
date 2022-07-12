import pytest
import os
import re

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks

# Values that help determine the running conditions
number_of_data_producers=2
run_duration=20  # seconds

# Default values for validation parameters
expected_number_of_data_files=1
check_for_logfile_errors=True
expected_event_count=run_duration
expected_event_count_tolerance=2
wib1_frag_hsi_trig_params={"fragment_type_description": "WIB",
                           "hdf5_detector_group": "TPC", "hdf5_region_prefix": "APA",
                           "expected_fragment_count": number_of_data_producers,
                           "min_size_bytes": 37200, "max_size_bytes": 37200}
wib1_frag_multi_trig_params={"fragment_type_description": "WIB",
                             "hdf5_detector_group": "TPC", "hdf5_region_prefix": "APA",
                             "expected_fragment_count": number_of_data_producers,
                             "min_size_bytes": 80, "max_size_bytes": 37200}
wib2_frag_params={"fragment_type_description": "WIB2",
                  "hdf5_detector_group": "TPC", "hdf5_region_prefix": "APA",
                  "expected_fragment_count": number_of_data_producers,
                  "min_size_bytes": 29000, "max_size_bytes": 30000}
pds_frag_params={"fragment_type_description": "PDS",
                 "hdf5_detector_group": "PDS", "hdf5_region_prefix": "Region",
                 "expected_fragment_count": number_of_data_producers,
                 "min_size_bytes": 80, "max_size_bytes": 36000}
triggercandidate_frag_params={"fragment_type_description": "Trigger Candidate",
                              "hdf5_detector_group": "Trigger", "hdf5_region_prefix": "Region",
                              "expected_fragment_count": 1,
                              "min_size_bytes": 130, "max_size_bytes": 150}
triggertp_frag_params={"fragment_type_description": "Trigger with TPs",
                       "hdf5_detector_group": "Trigger", "hdf5_region_prefix": "Region",
                       "expected_fragment_count": number_of_data_producers+2,
                       "min_size_bytes": 80, "max_size_bytes": 16000}
ignored_logfile_problems={"dqm": ["client will not be able to connect to Kafka cluster",
                                  "Unexpected Trigger Decision", "Unexpected Fragment"],
                          "trigger": ["zipped_tpset_q: Unable to push within timeout period"],
                          "ruemu": ["Configuration Error: Binary file contains more data than expected"],
                         }

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="daqconf_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
confgen_arguments_base=[ "-d", "./frames.bin", "-o", ".", "-s", "10", "-n", str(number_of_data_producers), "-b", "1000", "-a", "1000", "--host-ru", "localhost"]
confgen_arguments={"WIB1_System": confgen_arguments_base, 
                   "Software_TPG_System": confgen_arguments_base+["--enable-software-tpg"], 
                   "DQM_System": confgen_arguments_base+["--enable-dqm"], 
                   "WIB2_System": confgen_arguments_base+["--frontend-type", "wib2"], 
                   "PDS_(list)_System": confgen_arguments_base+["--frontend-type", "pds_list"], 
                   "PDS_(queue)_System": confgen_arguments_base+["--frontend-type", "pds_queue"]}
# The commands to run in nanorc, as a list
nanorc_command_list="integtest-partition boot init conf start 101 wait 1 resume wait ".split() + [str(run_duration)] + "pause wait 2 stop wait 2 scrap terminate".split()

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
    local_check_flag=check_for_logfile_errors

    if local_check_flag:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(run_nanorc.log_files, True, True, ignored_logfile_problems)

def test_data_files(run_nanorc):
    local_expected_event_count=expected_event_count
    local_event_count_tolerance=expected_event_count_tolerance
    fragment_check_list=[]
    if "--enable-software-tpg" in run_nanorc.confgen_arguments:
        local_expected_event_count+=(285*number_of_data_producers*run_duration/100)
        local_event_count_tolerance+=(10*number_of_data_producers*run_duration/100)
        fragment_check_list.append(wib1_frag_multi_trig_params)
        fragment_check_list.append(triggertp_frag_params)
    else:
        fragment_check_list.append(triggercandidate_frag_params)
        if "--frontend-type" in run_nanorc.confgen_arguments and \
           ("pds_list" in run_nanorc.confgen_arguments or \
            "pds_queue" in run_nanorc.confgen_arguments):
            fragment_check_list.append(pds_frag_params)
        elif "--frontend-type" in run_nanorc.confgen_arguments and \
           "wib2" in run_nanorc.confgen_arguments:
            fragment_check_list.append(wib2_frag_params)
        else:
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
