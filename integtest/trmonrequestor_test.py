import math
import pytest
import os
import re
import copy

import integrationtest.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.data_classes as data_classes

pytest_plugins = "integrationtest.integrationtest_drunc"

# Values that help determine the running conditions
number_of_data_producers = 2
run_duration = 20  # seconds
trigger_rate = 1.0  # Hz
data_rate_slowdown_factor = 1

# Default values for validation parameters
expected_number_of_data_files = 2
check_for_logfile_errors = True
expected_event_count = trigger_rate * run_duration
expected_event_count_tolerance = math.ceil(expected_event_count / 10)
wibeth_frag_hsi_trig_params = {
    "fragment_type_description": "WIBEth",
    "fragment_type": "WIBEth",
    "expected_fragment_count": (number_of_data_producers),
    "min_size_bytes": 7272,
    "max_size_bytes": 14472,
}
wibeth_frag_multi_trig_params = {
    "fragment_type_description": "WIBEth",
    "fragment_type": "WIBEth",
    "expected_fragment_count": (number_of_data_producers),
    "min_size_bytes": 72,
    "max_size_bytes": 14472,
}
triggercandidate_frag_params = {
    "fragment_type_description": "Trigger Candidate",
    "fragment_type": "Trigger_Candidate",
    "expected_fragment_count": 1,
    "min_size_bytes": 128,
    "max_size_bytes": 280,
}
triggeractivity_frag_params = {
    "fragment_type_description": "Trigger Activity",
    "fragment_type": "Trigger_Activity",
    "expected_fragment_count": 1,
    "min_size_bytes": 72,
    "max_size_bytes": 216,
}
triggerprimitive_frag_params = {
    "fragment_type_description": "Trigger Primitive",
    "fragment_type": "Trigger_Primitive",
    "expected_fragment_count": 2,  # number of readout apps (1) times 2
    "min_size_bytes": 72,
    "max_size_bytes": 16000,
}
hsi_frag_params = {
    "fragment_type_description": "HSI",
    "fragment_type": "Hardware_Signal",
    "expected_fragment_count": 1,
    "min_size_bytes": 72,
    "max_size_bytes": 100,
}
ignored_logfile_problems = {
    "-controller": [
        "Worker with pid \\d+ was terminated due to signal 1",
    ],
    "local-connection-server": [
        "errorlog: -",
        "Worker with pid \\d+ was terminated due to signal 1",
    ],
    "log_.*_disabled_": ["connect: Connection refused"],
}

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

object_databases = [ "config/daqsystemtest/integrationtest-objects.data.xml" ]

conf_dict = data_classes.drunc_config()
conf_dict.dro_map_config.n_streams = number_of_data_producers
conf_dict.op_env = "integtest"
conf_dict.session = "trmonrequestor"
conf_dict.tpg_enabled = False
conf_dict.trmon_app_enabled = True
conf_dict.n_df_apps = 2


substitution = data_classes.config_substitution(
    obj_id="random-tc-generator",
    obj_class="RandomTCMakerConf",
    updates={"trigger_rate_hz": trigger_rate},
)
conf_dict.config_substitutions.append(substitution)

confgen_arguments = {
    "WIBEth_System": conf_dict,
}

# The commands to run in nanorc, as a list
nanorc_command_list = (
    "boot conf start --run-number 101 wait 1 enable-triggers wait ".split()
    + [str(run_duration)]
    + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop scrap terminate".split()
)

# The tests themselves


def test_nanorc_success(run_nanorc):
    current_test = os.environ.get("PYTEST_CURRENT_TEST")
    match_obj = re.search(r".*\[(.+)-run_nanorc0\].*", current_test)
    if match_obj:
        current_test = match_obj.group(1)
    banner_line = re.sub(".", "=", current_test)
    print(banner_line)
    print(current_test)
    print(banner_line)
    # Check that nanorc completed correctly
    assert run_nanorc.completed_process.returncode == 0


def test_log_files(run_nanorc):
    if check_for_logfile_errors:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(
            run_nanorc.log_files, True, True, ignored_logfile_problems
        )


def test_data_files(run_nanorc):
    local_expected_event_count = expected_event_count
    local_event_count_tolerance = expected_event_count_tolerance
    fragment_check_list = [triggercandidate_frag_params, hsi_frag_params]
    if run_nanorc.confgen_config.tpg_enabled:
        local_expected_event_count += (
            250 * number_of_data_producers * run_duration / 100
        )
        local_event_count_tolerance += (
            10 * number_of_data_producers * run_duration / 100
        )
        fragment_check_list.append(wibeth_frag_multi_trig_params)  # WIBEth
        fragment_check_list.append(triggerprimitive_frag_params)
        fragment_check_list.append(triggeractivity_frag_params)
    else:
        fragment_check_list.append(wibeth_frag_hsi_trig_params)  # WIBEth

    # Run some tests on the output data file
    assert len(run_nanorc.data_files) == expected_number_of_data_files

    all_ok = True
    for idx in range(len(run_nanorc.data_files)):
        data_file = data_file_checks.DataFile(run_nanorc.data_files[idx])
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
