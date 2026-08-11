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

# Values that help determine the running conditions
output_path_parameter = "."
number_of_data_producers = 10
run_duration = 32  # seconds
number_of_readout_apps = 3
number_of_dataflow_apps = 1
trigger_rate = 0.06  # Hz
readout_window_time_before = 10000000
readout_window_time_after = 1000000

# Default values for validation parameters
expected_number_of_data_files = 4
check_for_logfile_errors = True
expected_event_count = 1
expected_event_count_tolerance = 1
wibeth_frag_55pct_params = {
    "fragment_type_description": "WIBEth",
    "fragment_type": "WIBEth",
    "expected_fragment_count": (number_of_data_producers * number_of_readout_apps),
    "min_size_bytes": 38678472,
    "max_size_bytes": 38685672,
}
wibeth_frag_125pct_params = {
    "fragment_type_description": "WIBEth",
    "fragment_type": "WIBEth",
    "expected_fragment_count": (number_of_data_producers * number_of_readout_apps),
    "min_size_bytes": 91411272,
    "max_size_bytes": 91418472,
}
triggercandidate_frag_params = {
    "fragment_type_description": "Trigger Candidate",
    "fragment_type": "Trigger_Candidate",
    "expected_fragment_count": 1,
    "min_size_bytes": 128,
    "max_size_bytes": 128,
}
ignored_logfile_problems = {
    "-controller": [
        "Worker with pid \\d+ was terminated due to signal 1",
    ],
    "connectivity-service": [
        "errorlog: -",
        "Worker with pid \\d+ was terminated due to signal 1",
    ],
}

# Determine if the conditions are right for these tests
resource_validator = resource_validation.ResourceValidator()
resource_validator.cpu_count_needs(12, 24)  # 1/3 for each data source (30) plus two more for everything else
resource_validator.free_memory_needs(28, 40)  # 20% more than what we observe being used ('free -h')
resource_validator.total_memory_needs()  # no specific request, but it's useful to see how much is available
actual_output_path = get_pytest_tmpdir()
# The largest data set in this test is four 2.55 GB files.  To handle these four files
# plus a safety factor of three for the last one, we need 6*2.55 = 15.3.
# Note that a safety factor of 3 is configured below, over-riding the default value of 5.
resource_validator.free_disk_space_needs(actual_output_path, 16, 20)
resource_validator.total_disk_space_needs(actual_output_path, recommended_total_disk_space=24)


conf_dict = data_classes.integtest_params_for_generated_dunedaq_config()
conf_dict.object_databases = ["config/daqsystemtest/integrationtest-objects.data.xml"]
conf_dict.dro_map_config.n_streams = number_of_data_producers
conf_dict.dro_map_config.n_apps = number_of_readout_apps
conf_dict.op_env = "integtest"
conf_dict.config_session_name= "largerecord"
conf_dict.tpg_enabled = False
conf_dict.n_df_apps = number_of_dataflow_apps
conf_dict.remove_hdf5_files = True
utility_functions.set_rtcm_trigger_params(conf_dict, trigger_rate=trigger_rate,
                                          readout_window_backshift_ticks=0,
                                          readout_window_before_ticks=readout_window_time_before,
                                          readout_window_after_ticks=readout_window_time_after)

conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="DataStoreConf",
        obj_id="default",
        updates={
            "max_file_size": 2 * 1024 * 1024 * 1024,
            "directory_path": output_path_parameter,
            "free_space_safety_factor": 3,
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
        obj_class="LatencyBuffer", updates={"size": 100000}
    )
)
conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="TRBConf",
        updates={
            "trigger_record_timeout_ms": 1000 / trigger_rate
        },
    )
)
oversize_conf = copy.deepcopy(conf_dict)  # Copy before setting the readout window
utility_functions.set_rtcm_trigger_params(oversize_conf, trigger_rate=trigger_rate,
                                          readout_window_backshift_ticks=0,
                                          readout_window_before_ticks=int(2.5*readout_window_time_before),
                                          readout_window_after_ticks=readout_window_time_after)

confgen_arguments = {
    "TRSize_55PercentOfMaxFileSize": conf_dict,
    "TRSize_125PercentOfMaxFileSize": oversize_conf,
}
# The commands to run in dunerc, as a list
dunerc_command_list = (
    "boot conf wait 5".split()
    + "start --run-number 101 wait 10 enable-triggers wait ".split()
    + [str(run_duration)]
    + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop ".split()
    + "start --run-number 102 wait 10 enable-triggers wait ".split()
    + [str(run_duration)]
    + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop ".split()
    + " scrap terminate".split()
)

# The tests themselves


def test_dunerc_success(run_dunerc, caplog):
    # checks for run control success, problems during pytest setup, etc.
    utility_functions.basic_checks(run_dunerc, caplog, print_test_name=True)


def test_log_files(run_dunerc):
    if check_for_logfile_errors:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(
            run_dunerc.log_files, True, True, ignored_logfile_problems,
            verbosity_helper=run_dunerc.verbosity_helper
        )


def test_data_files(run_dunerc):
    local_expected_event_count = expected_event_count
    local_event_count_tolerance = expected_event_count_tolerance
    fragment_check_list = [triggercandidate_frag_params]
    current_test = os.environ.get("PYTEST_CURRENT_TEST")
    # 30-Dec-2025, KAB: modified the following "if" statement to check if the desired
    # configuration name is contained in the current test name (instead of using an "if"
    # clause that tests if the first part of the current test name is *equal* to the config
    # name). This change is needed now because current test name has more information
    # in it, and a simple comparison no longer works.
    if "TRSize_125PercentOfMaxFileSize" in current_test:
        fragment_check_list.append(wibeth_frag_125pct_params)
    else:
        fragment_check_list.append(wibeth_frag_55pct_params)

    # Run some tests on the output data file
    all_ok = len(run_dunerc.data_files) == expected_number_of_data_files
    #print("") # Clear potential dot from pytest
    if all_ok:
        if run_dunerc.verbosity_helper.compare_level(IntegtestVerbosityLevels.drunc_transitions):
            print(f"\N{WHITE HEAVY CHECK MARK} The correct number of raw data files was found ({expected_number_of_data_files})")
    else:
        print(f"\N{POLICE CARS REVOLVING LIGHT} An incorrect number of raw data files was found, expected {expected_number_of_data_files}, found {len(run_dunerc.data_files)} \N{POLICE CARS REVOLVING LIGHT}")

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
