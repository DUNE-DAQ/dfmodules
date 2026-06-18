import pytest
import os
import re
import urllib.request

import integrationtest.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.data_classes as data_classes
import integrationtest.resource_validation as resource_validation
import integrationtest.utility_functions as utility_functions
from integrationtest.get_pytest_tmpdir import get_pytest_tmpdir
from integrationtest.verbosity_helper import IntegtestVerbosityLevels

import functools
print = functools.partial(print, flush=True)  # always flush print() output

pytest_plugins = "integrationtest.integrationtest_drunc"

# 21-Jul-2022, KAB:
# --> problems in the C++ code that this script currently highlights
# * the crash of the DF App in the second run
# * the need for the HDF5DataStore to stop retrying writes at stop (drain-dataflow?) time

# Values that help determine the running conditions
output_path_parameter = "."
desired_size_of_output_disk_gb = 6
minimum_free_disk_space_gb = desired_size_of_output_disk_gb + 5  # leave 5 GB free for other users/integtests/etc
number_of_data_producers = 10
run_duration = 20  # seconds
number_of_readout_apps = 3
number_of_dataflow_apps = 1
trigger_rate = 0.2  # Hz
readout_window_time_before = 9000000
readout_window_time_after = 1000000

# Default values for validation parameters
expected_number_of_data_files = 2
check_for_logfile_errors = True
expected_event_count = 2 # files can have 1 to 4 TriggerRecords, so we allow 2 +- 2
expected_event_count_tolerance = 2

wibeth_frag_hsi_trig_params = {
    "fragment_type_description": "WIBEth",
    "fragment_type": "WIBEth",
    "expected_fragment_count": (number_of_data_producers * number_of_readout_apps),
    "min_size_bytes": 35157672,
    "max_size_bytes": 35164872,
}
triggercandidate_frag_params = {
    "fragment_type_description": "Trigger Candidate",
    "fragment_type": "Trigger_Candidate",
    "expected_fragment_count": 1,
    "min_size_bytes": 128,
    "max_size_bytes": 280,
}
required_logfile_problems = {
    "df-01": [
        "A problem was encountered when writing TriggerRecord number",
        "A problem was encountered when writing a trigger record to file",
        r"There are \d+ bytes free, and the required minimum is \d+ bytes based on a safety factor of \d+ times the trigger record size",
    ],
    "mlt": [r"Trigger is inhibited in run \d+"],
    "dfo": [r"TriggerDecision \d+ didn't complete within timeout in run \d+"],
}
ignored_logfile_problems = {
    "connectivity-service": [
        "errorlog: -",
    ],
}

# Determine if the conditions are right for these tests
resource_validator = resource_validation.ResourceValidator()
resource_validator.cpu_count_needs(12, 24)  # 1/3 for each data source (30) plus two more for everything else
resource_validator.free_memory_needs(50, 70)  # 10% more than what we observe being used ('free -h')
resource_validator.total_memory_needs()  # no specific request, but it's useful to see how much is available
actual_output_path = get_pytest_tmpdir()
resource_validator.free_disk_space_needs(actual_output_path, minimum_free_disk_space_gb)
resource_validator.total_disk_space_needs(actual_output_path,
                                          recommended_total_disk_space=2*minimum_free_disk_space_gb)  # double what we need

# We simulate a nearly-full output disk by setting the free-space-safety-factor
# that the data writer uses to a custom value, based on the free space on disk.
# The size of each TriggerRecord in this test is tuned to be about 1 GB, so if
# the free-space-safety-factor is calculated to be 10, then it will appear that
# the disk is full when there is still ~< 10 GB of free space.  And, having a
# 1 GB size for the TRs means that we will write approximately
# desired_free_disk_space_gb TriggerRecords before appearing to run out of space.
free_space_safety_factor = round(resource_validator.free_disk_space_gb - desired_size_of_output_disk_gb)


conf_dict = data_classes.integtest_params_for_generated_dunedaq_config()
conf_dict.object_databases = ["config/daqsystemtest/integrationtest-objects.data.xml"]
conf_dict.dro_map_config.n_streams = number_of_data_producers
conf_dict.dro_map_config.n_apps = number_of_readout_apps
conf_dict.op_env = "integtest"
conf_dict.config_session_name= "insufficient"
conf_dict.tpg_enabled = False
conf_dict.n_df_apps = number_of_dataflow_apps
conf_dict.fake_hsi_enabled = False
conf_dict.remove_hdf5_files = True

conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="RandomTCMakerConf",
        updates={
            "trigger_rate_hz": trigger_rate,
            "candidate_backshift_ts": 0,
            "candidate_window_before_ts": readout_window_time_before,
            "candidate_window_after_ts": readout_window_time_after
        },
    )
)

conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="DataStoreConf",
        obj_id="default",
        updates={
            "directory_path": output_path_parameter,
            "free_space_safety_factor": free_space_safety_factor,
        },
    )
)
conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="DFOConf", updates={"busy_threshold": 1, "free_threshold": 0}
    )
)
conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="LatencyBuffer", updates={"size": 200000}
    )
)

confgen_arguments = {
    "Base_System": conf_dict,
}
# The commands to run in dunerc, as a list
dunerc_command_list = (
    "boot conf wait 5".split()
    + "start --run-number 101 wait 1 enable-triggers wait ".split()
    + [str(run_duration)]
    + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop ".split()
    + "start --run-number 102 wait 1 enable-triggers wait ".split()
    + [str(run_duration)]
    + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop ".split()
    + "start --run-number 103 wait 1 enable-triggers wait ".split()
    + [str(run_duration)]
    + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop ".split()
    + " scrap terminate".split()
)

# The tests themselves


def test_dunerc_success(run_dunerc, caplog):
    # checks for run control success, problems during pytest setup, etc.
    utility_functions.basic_checks(run_dunerc, caplog, print_test_name=False)


def test_log_files(run_dunerc):
    if check_for_logfile_errors:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(
            run_dunerc.log_files,
            True,
            True,
            ignored_logfile_problems,
            required_logfile_problems,
            verbosity_helper=run_dunerc.verbosity_helper
        )


def test_data_files(run_dunerc):
    local_expected_event_count = expected_event_count
    local_event_count_tolerance = expected_event_count_tolerance
    fragment_check_list = [triggercandidate_frag_params]
    # fragment_check_list.append(wib1_frag_hsi_trig_params)
    # fragment_check_list.append(wib2_frag_hsi_trig_params) # DuneWIB
    fragment_check_list.append(wibeth_frag_hsi_trig_params)  # WIBEth

    # Run some tests on the output data file
    all_ok = len(run_dunerc.data_files) == expected_number_of_data_files or len(run_dunerc.data_files) == (expected_number_of_data_files+1)
    #print("") # Clear potential dot from pytest
    if all_ok:
        if run_dunerc.verbosity_helper.compare_level(IntegtestVerbosityLevels.drunc_transitions):
            print(f"\N{WHITE HEAVY CHECK MARK} An acceptable number of raw data files was found ({len(run_dunerc.data_files)} in {expected_number_of_data_files}..{expected_number_of_data_files+1})")
    else:
        print(f"\N{POLICE CARS REVOLVING LIGHT} An incorrect number of raw data files was found, expected {expected_number_of_data_files}..{expected_number_of_data_files+1}, found {len(run_dunerc.data_files)} \N{POLICE CARS REVOLVING LIGHT}")

    for idx in range(len(run_dunerc.data_files)):
        data_file = data_file_checks.DataFile(run_dunerc.data_files[idx], run_dunerc.verbosity_helper)
        all_ok &= data_file_checks.sanity_check(data_file)
        all_ok &= data_file_checks.check_file_attributes(data_file)
        all_ok &= data_file_checks.check_event_count(
            data_file, local_expected_event_count, local_event_count_tolerance
        )
        for jdx in range(len(fragment_check_list)):
            all_ok &= data_file_checks.check_fragment_count(
                data_file, fragment_check_list[jdx]
            )
            all_ok &= data_file_checks.check_fragment_sizes(
                data_file, fragment_check_list[jdx]
            )
    assert all_ok
