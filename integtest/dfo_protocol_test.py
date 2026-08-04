"""
Integration Test for DFO Protocol

This test module validates DAQ system behavior while using multiple DFO applications.
It also verifies that the protocol correctly handles DFO and DF application crashes.
"""

import copy
import conffwk
import os
import pathlib
import pytest
import random
import string

import integrationtest.data_classes as data_classes
import integrationtest.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.resource_validation as resource_validation
import integrationtest.utility_functions as utility_functions
from integrationtest.get_pytest_tmpdir import get_pytest_tmpdir
from integrationtest.verbosity_helper import IntegtestVerbosityLevels

import functools

print = functools.partial(print, flush=True)  # always flush print() output

pytest_plugins = "integrationtest.integrationtest_drunc"

# Run setup
run_duration = 30  # seconds
check_for_logfile_errors = True

# Default values for validation parameters
number_of_dataflow_apps = 3
number_of_data_producers = 4
number_of_readout_apps = 2
trigger_rate = 4.0
expected_number_of_data_files = number_of_dataflow_apps
check_for_logfile_errors = True
expected_event_count = run_duration * trigger_rate / number_of_dataflow_apps
expected_event_count_tolerance = expected_event_count / 10
ta_prescale = 1000

wibeth_frag_params = {
    "fragment_type_description": "WIBEth",
    "fragment_type": "WIBEth",
    "expected_fragment_count": number_of_readout_apps * number_of_data_producers,
    "min_size_bytes": 7272,
    "max_size_bytes": 28872,
}
# sizes: 128 is for one TC with zero TAs inside it (72+56)
#        208 is for one TC with one TA inside it (72+56+80)
#        264 is for two TCs with one TA in one of them (72+56+80+56)
triggercandidate_frag_params = {
    "fragment_type_description": "Trigger Candidate",
    "fragment_type": "Trigger_Candidate",
    "expected_fragment_count": 1,
    "min_size_bytes": 128,
    "max_size_bytes": 264,
    "debug_mask": 0x0,
    "frag_sizes_by_TC_type": {"kPrescale": {"min_size_bytes": 208, "max_size_bytes": 264},
                                "kRandom": {"min_size_bytes": 128, "max_size_bytes": 264},
                                "default": {"min_size_bytes": 128, "max_size_bytes": 264} }
}
# sizes:  72 is for an empty TP fragment
#        168 is for a fragment with four TPs in it (72+24+24+24+24)
triggerprimitive_frag_params = {
    "fragment_type_description": "Trigger Primitive",
    "fragment_type": "Trigger_Primitive",
    "expected_fragment_count": 3 * number_of_readout_apps,
    "min_size_bytes": 72,
    "max_size_bytes": 168,
}
# 03-Jul-2025, KAB: changing the default max size from 72 to 100 to handle cases in which there
# was a Random or Prescale trigger along with a coincidental HSI event within the readout window.
hsi_frag_params = {
    "fragment_type_description": "HSI",
    "fragment_type": "Hardware_Signal",
    "expected_fragment_count": 1,
    "min_size_bytes": 72,
    "max_size_bytes": 100,
    "frag_sizes_by_TC_type": {"kTiming": {"min_size_bytes": 100, "max_size_bytes": 100},
                              "default": {"min_size_bytes":  72, "max_size_bytes": 100} }
}
ignored_logfile_problems = {
    "-controller": [
    ],
    "local-connection-server": [
        "errorlog: -",
    ],
    # 04-Mar-2026, KAB: added the absl::InitializeLog warning message to the ignored list for
    # all DAQ processes, given that we currently don't have a way suppress it at its source.
    r".*": [
        r"WARNING: All log messages before absl::InitializeLog\(\) is called are written to STDERR"
    ]
}

# Determine if this computer has enough resources for these tests
resource_validator = resource_validation.ResourceValidator()
resource_validator.cpu_count_needs(
    15, 30
)  # 3 for each data source (incl TPG) plus 3 more for everything else
resource_validator.free_memory_needs(
    9, 14
)  # 30% more than what we observe being used ('free -h')
actual_output_path = get_pytest_tmpdir()
resource_validator.free_disk_space_needs(
    actual_output_path, 1
)  # more than what we observe

### Config setup
common_config_obj = data_classes.integtest_params_for_predefined_dunedaq_config()
common_config_obj.op_env = "test"
common_config_obj.predefined_config_db ="config/daqsystemtest/example-configs.data.xml"

common_config_obj.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="TCDataProcessor",     # 12-Nov-2025, KAB: turned off the merging of
        obj_id="def-tc-processor",       # overlapping TCs so that we get more consistent
        updates={                        # numbers of TriggerRecords in the output files.
            "merge_overlapping_tcs": False
        },)
)

# Get default config
multidfo_local_conf = copy.deepcopy(common_config_obj)
multidfo_local_conf.config_session_name = "local-multidfo-config"

# Prep configs
stopped_dfo_conf = copy.deepcopy(multidfo_local_conf)
killed_dfo_conf = copy.deepcopy(multidfo_local_conf)
stopped_df_conf = copy.deepcopy(multidfo_local_conf)
killed_df_conf = copy.deepcopy(multidfo_local_conf)

stopped_dfo_conf.system_signal_configs = [
    data_classes.system_signal_config(
        application_label="dfo-02", signal=data_classes.PosixSignal.SIGSTOP, delay_s=25
    ),
    data_classes.system_signal_config(
        application_label="dfo-02", signal=data_classes.PosixSignal.SIGCONT, delay_s=30
    ),
]


killed_dfo_conf.system_signal_configs = [
    data_classes.system_signal_config(
        application_label="dfo-02", signal=data_classes.PosixSignal.SIGKILL, delay_s=25
    ),
]

stopped_df_conf.system_signal_configs = [
    data_classes.system_signal_config(
        application_label="df-02", signal=data_classes.PosixSignal.SIGSTOP, delay_s=25
    ),
    data_classes.system_signal_config(
        application_label="df-02", signal=data_classes.PosixSignal.SIGCONT, delay_s=30
    ),
]

killed_df_conf.system_signal_configs = [
    data_classes.system_signal_config(
        application_label="df-02", signal=data_classes.PosixSignal.SIGKILL, delay_s=25
    ),
]

# Finally store configs in map
confgen_arguments = {
    "default": multidfo_local_conf,
    "stopped-dfo": stopped_dfo_conf,
    "killed-dfo": killed_dfo_conf,
    "stopped-df": stopped_df_conf,
    "killed-df": killed_df_conf,
}

# The commands to run in dunerc, as a list
dunerc_command_list = (
    "boot wait 2 conf start --run-number 101 wait 1 enable-triggers wait ".split()
    + [str(run_duration)]
    + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop scrap terminate".split()
)


### Tests

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
    low_number_of_files = expected_number_of_data_files
    high_number_of_files = expected_number_of_data_files
    fragment_check_list = [triggercandidate_frag_params, hsi_frag_params, wibeth_frag_params]

    local_expected_event_count += (
        (6250.0 / ta_prescale)
        * number_of_data_producers
        * number_of_readout_apps
        * run_duration
        / (100 * number_of_dataflow_apps)
    )
    local_event_count_tolerance += (
        (250.0 / ta_prescale)
        * number_of_data_producers
        * number_of_readout_apps
        * run_duration
        / (100 * number_of_dataflow_apps)
    )
    fragment_check_list.append(triggerprimitive_frag_params)
    nontrig_fragment_check_list = [hsi_frag_params, wibeth_frag_params]

    # Run some tests on the output data file
    assert (
        len(run_dunerc.data_files) == high_number_of_files
        or len(run_dunerc.data_files) == low_number_of_files
    )

    all_ok = True
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
        for kdx in range(len(nontrig_fragment_check_list)):
            all_ok &= data_file_checks.check_fragment_error_flags( data_file, nontrig_fragment_check_list[kdx])
    assert all_ok
