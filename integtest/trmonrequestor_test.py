import math
import pytest
import os
import re

import integrationtest.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.data_classes as data_classes
import integrationtest.utility_functions as utility_functions
from integrationtest.verbosity_helper import IntegtestVerbosityLevels

import functools
print = functools.partial(print, flush=True)  # always flush print() output

pytest_plugins = "integrationtest.integrationtest_drunc"

# Values that help determine the running conditions
number_of_data_producers = 2
number_of_dataflow_apps = 2
run_duration = 20  # seconds
trigger_rate = 1.0  # Hz
trmon_prescale = 3

# Default values for validation parameters
run_count = 3
expected_number_of_data_files = 2 * run_count
check_for_logfile_errors = True
expected_event_count = trigger_rate * run_duration / number_of_dataflow_apps
expected_event_count_tolerance = math.ceil(expected_event_count / 10)
wibeth_frag_params = {
    "fragment_type_description": "WIBEth",
    "fragment_type": "WIBEth",
    "expected_fragment_count": number_of_data_producers,
    "min_size_bytes": 7272,
    "max_size_bytes": 14472,
}
triggercandidate_frag_params = {
    "fragment_type_description": "Trigger Candidate",
    "fragment_type": "Trigger_Candidate",
    "expected_fragment_count": 1,
    "min_size_bytes": 128,
    "max_size_bytes": 216,
}
hsi_frag_params = {
    "fragment_type_description": "HSI",
    "fragment_type": "Hardware_Signal",
    "expected_fragment_count": 0,
    "min_size_bytes": 72,
    "max_size_bytes": 100,
}
ignored_logfile_problems = {
    "-controller": [
        "Worker with pid \\d+ was terminated due to signal",
        "Connection '.*' not found on the application registry",
    ],
    "connectivity-service": [
        "errorlog: -",
    ],
}


conf_dict = data_classes.integtest_params_for_generated_dunedaq_config()
conf_dict.object_databases = ["config/daqsystemtest/integrationtest-objects.data.xml"]
conf_dict.dro_map_config.n_streams = number_of_data_producers
conf_dict.op_env = "integtest"
conf_dict.config_session_name= "trmonrequestor"
conf_dict.tpg_enabled = False
conf_dict.trmon_app_enabled = True
conf_dict.n_df_apps = number_of_dataflow_apps
utility_functions.set_rtcm_trigger_params(conf_dict, trigger_rate=trigger_rate,
                                          readout_window_backshift_ticks=0)

substitution = data_classes.attribute_substitution(
    obj_id="tr_mon_dw-01",
    obj_class="DataWriterConf",
    updates={"data_storage_prescale": trmon_prescale},
)
conf_dict.config_substitutions.append(substitution)

confgen_arguments = {
    "WIBEth_System": conf_dict,
}

# The commands to run in dunerc, as a list
def make_run_command_list(runnum):
    return (
        f"start --run-number {runnum} wait 1 enable-triggers wait ".split()
        + [str(run_duration)]
        + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop ".split())

dunerc_command_list = "boot conf wait 5".split()
for ii in range(run_count):
    dunerc_command_list += make_run_command_list(100 + ii)
dunerc_command_list += " scrap terminate".split()

# The tests themselves
def test_dunerc_success(run_dunerc, caplog):
    # checks for run control success, problems during pytest setup, etc.
    utility_functions.basic_checks(run_dunerc, caplog, print_test_name=False)


def test_log_files(run_dunerc):

    # Check that at least some of the expected log files are present
    assert any(
        f"{run_dunerc.daq_session_name}_df-01" in str(logname)
        for logname in run_dunerc.log_files
    )
    assert any(
        f"{run_dunerc.daq_session_name}_dfo" in str(logname) for logname in run_dunerc.log_files
    )
    assert any(
        f"{run_dunerc.daq_session_name}_mlt" in str(logname) for logname in run_dunerc.log_files
    )
    assert any(
        f"{run_dunerc.daq_session_name}_ru" in str(logname) for logname in run_dunerc.log_files
    )

    if check_for_logfile_errors:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(
            run_dunerc.log_files, True, True, ignored_logfile_problems,
            verbosity_helper=run_dunerc.verbosity_helper
        )


def test_data_files(run_dunerc):
    # Run some tests on the output data file
    all_ok = len(run_dunerc.data_files) == expected_number_of_data_files
    #print("")  # Clear potential dot from pytest
    if all_ok:
        if run_dunerc.verbosity_helper.compare_level(IntegtestVerbosityLevels.drunc_transitions):
            print(
                f"\N{WHITE HEAVY CHECK MARK} The correct number of raw data files was found ({expected_number_of_data_files})"
            )
    else:
        print(
            f"\N{POLICE CARS REVOLVING LIGHT} An incorrect number of raw data files was found, expected {expected_number_of_data_files}, found {len(run_dunerc.data_files)} \N{POLICE CARS REVOLVING LIGHT}"
        )

    fragment_check_list = [triggercandidate_frag_params, hsi_frag_params]
    fragment_check_list.append(wibeth_frag_params)
    nontrig_fragment_check_list = [hsi_frag_params, wibeth_frag_params]

    for idx in range(len(run_dunerc.data_files)):
        data_file = data_file_checks.DataFile(run_dunerc.data_files[idx], run_dunerc.verbosity_helper)
        all_ok &= data_file_checks.sanity_check(data_file)
        all_ok &= data_file_checks.check_file_attributes(data_file)
        all_ok &= data_file_checks.check_event_count(
            data_file, expected_event_count, expected_event_count_tolerance
        )
        for jdx in range(len(fragment_check_list)):
            all_ok &= data_file_checks.check_fragment_count(
                data_file, fragment_check_list[jdx]
            )
            all_ok &= data_file_checks.check_fragment_sizes(
                data_file, fragment_check_list[jdx]
            )
        for kdx in range(len(nontrig_fragment_check_list)):
            all_ok &= data_file_checks.check_fragment_error_flags(
                data_file, nontrig_fragment_check_list[kdx]
            )

    trmon_low = (run_count * expected_event_count * number_of_dataflow_apps / trmon_prescale) - 1
    trmon_high = (run_count * expected_event_count * number_of_dataflow_apps / trmon_prescale) + 2
    trmon_ok = len(run_dunerc.trmon_files) > trmon_low
    trmon_ok &= len(run_dunerc.trmon_files) < trmon_high

    print("")  # Add break from Data file printouts
    if trmon_ok:
        if run_dunerc.verbosity_helper.compare_level(IntegtestVerbosityLevels.drunc_transitions):
            print(
                f"\N{WHITE HEAVY CHECK MARK} The correct number of TRMon data files was found ({trmon_high} > {len(run_dunerc.trmon_files)} > {trmon_low})"
            )
    else:
        print(
            f"\N{POLICE CARS REVOLVING LIGHT} An incorrect number of TRMon data files was found, expected between {trmon_low} and {trmon_high}, found {len(run_dunerc.trmon_files)} \N{POLICE CARS REVOLVING LIGHT}"
        )

    for idx in range(len(run_dunerc.trmon_files)):
        data_file = data_file_checks.DataFile(run_dunerc.trmon_files[idx], run_dunerc.verbosity_helper)
        all_ok &= data_file_checks.sanity_check(data_file)
        all_ok &= data_file_checks.check_file_attributes(data_file)
        all_ok &= data_file_checks.check_event_count(data_file, 1, 0)
        for jdx in range(len(fragment_check_list)):
            all_ok &= data_file_checks.check_fragment_count(
                data_file, fragment_check_list[jdx]
            )
            all_ok &= data_file_checks.check_fragment_sizes(
                data_file, fragment_check_list[jdx]
            )
        for kdx in range(len(nontrig_fragment_check_list)):
            all_ok &= data_file_checks.check_fragment_error_flags(
                data_file, nontrig_fragment_check_list[kdx]
            )

    assert all_ok
    assert trmon_ok
