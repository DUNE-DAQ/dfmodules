import pytest
import os
import re
import urllib.request

import integrationtest.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.data_classes as data_classes
import integrationtest.resource_validation as resource_validation

pytest_plugins = "integrationtest.integrationtest_drunc"

# Values that help determine the running conditions
number_of_data_producers = 2
number_of_readout_apps = 3

# Default values for validation parameters
check_for_logfile_errors = True

wibeth_frag_params = {
    "fragment_type_description": "WIBEth",
    "fragment_type": "WIBEth",
    "expected_fragment_count": (number_of_data_producers * number_of_readout_apps),
    "min_size_bytes": 187272,
    "max_size_bytes": 194472,
}
# sizes: 128 is for one TC with zero TAs inside it (72+56)
#        208 is for one TC with one TA inside it (72+56+80)
#        264 is for two TCs with one TA in one of them (72+56+80+56)
triggercandidate_frag_params = {
    "fragment_type_description": "Trigger Candidate",
    "fragment_type": "Trigger_Candidate",
    "expected_fragment_count": 1,
    "min_size_bytes": 128,
    "max_size_bytes": 128,
}
triggeractivity_frag_params = {
    "fragment_type_description": "Trigger Activity",
    "fragment_type": "Trigger_Activity",
    "expected_fragment_count": 0,
    "min_size_bytes": 72,
    "max_size_bytes": 632,
}
triggerprimitive_frag_params = {
    "fragment_type_description": "Trigger Primitive",
    "fragment_type": "Trigger_Primitive",
    "expected_fragment_count": 0,
    "min_size_bytes": 72,
    "max_size_bytes": 1032,
}
hsi_frag_params = {
    "fragment_type_description": "HSI",
    "fragment_type": "Hardware_Signal",
    "expected_fragment_count": 1,
    "min_size_bytes": 100,
    "max_size_bytes": 100,
}
ignored_logfile_problems = {
    "-controller": [
        "Worker with pid \\d+ was terminated due to signal 1",
    ],
    "connectivity-service": [
        "errorlog: -",
    ],
}

# Determine if the conditions are right for this testing
resval = resource_validation.ResourceValidator()
resval.require_cpu_count(10)  # one per data source, plus several spares
resval.require_free_memory_gb(10)  # approximately what we observe
resval.require_total_memory_gb(20)  # safety factor of 2
actual_output_path = "/tmp"
resval.require_free_disk_space_gb(actual_output_path, 1)  # what we observe
resval_debug_string = resval.get_debug_string()
print(f"{resval_debug_string}")

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

object_databases = ["config/daqsystemtest/integrationtest-objects.data.xml"]

conf_dict = data_classes.drunc_config()
conf_dict.dro_map_config.n_streams = number_of_data_producers
conf_dict.dro_map_config.n_apps = number_of_readout_apps
conf_dict.op_env = "integtest"
conf_dict.config_session_name= "multidatawriter"
conf_dict.fake_hsi_enabled = True
conf_dict.n_data_writers = 3

conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="FakeHSIEventGeneratorConf",
        updates={"trigger_rate": 10.0},
    )
)

conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="HSISignalWindow",
        updates={
            "time_before": 1000,
            "time_after": 500,
        },
    )
)
conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="TCReadoutMap",
        obj_id = "def-hsi-tc-map",
        updates={
            "time_before": 52000,
            "time_after": 1000,
        },
    )
)

conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="QueueDescriptor",
        obj_id="trigger-records",
        updates={"queue_type": "kFollyMPMCQueue"},
    )
)


confgen_arguments = {
    "WIBEth_System": conf_dict,
}

# The commands to run in nanorc, as a list
if resval.this_computer_has_sufficient_resources:
    nanorc_command_list = (
        "boot conf wait 5".split()
        + "start --run-number 101 wait 1 enable-triggers wait 30".split()
        + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop ".split()
        + "start --run-number 102 wait 1 enable-triggers wait 30".split()
        + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop ".split()
        + " scrap terminate".split()
    )
else:
    nanorc_command_list = ["wait", "1"]

# The tests themselves

def test_nanorc_success(run_nanorc):
    if not resval.this_computer_has_sufficient_resources:
        resval_report_string = resval.get_insufficient_resources_report()
        print(f"{resval_report_string}")
        resval_summary_string = resval.get_insufficient_resources_summary()
        pytest.skip(f"{resval_summary_string}")

    # print the name of the current test
    current_test = os.environ.get("PYTEST_CURRENT_TEST")
    match_obj = re.search(r".*\[(.+)-run_.*rc.*\d].*", current_test)
    if match_obj:
        current_test = match_obj.group(1)
    banner_line = re.sub(".", "=", current_test)
    print(banner_line)
    print(current_test)
    print(banner_line)

    # Check that nanorc completed correctly
    assert run_nanorc.completed_process.returncode == 0


def test_log_files(run_nanorc):
    if not resval.this_computer_has_sufficient_resources:
        resval_summary_string = resval.get_insufficient_resources_summary()
        pytest.skip(f"{resval_summary_string}")

    if check_for_logfile_errors:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(
            run_nanorc.log_files, True, True, ignored_logfile_problems
        )


def test_data_files(run_nanorc):
    if not resval.this_computer_has_sufficient_resources:
        resval_summary_string = resval.get_insufficient_resources_summary()
        pytest.skip(f"{resval_summary_string}")

    fragment_check_list = [triggercandidate_frag_params, hsi_frag_params, wibeth_frag_params]
    fragment_check_list.append(triggerprimitive_frag_params)
    fragment_check_list.append(triggeractivity_frag_params)

    # Run some tests on the output data file
    all_ok = len(run_nanorc.data_files) == 6  # three for each run
    print("") # Clear potential dot from pytest
    if all_ok:
        print("\N{WHITE HEAVY CHECK MARK} The correct number of raw data files was found (6)")
    else:
        print(f"\N{POLICE CARS REVOLVING LIGHT} An incorrect number of raw data files was found, expected 6, found {len(run_nanorc.data_files)} \N{POLICE CARS REVOLVING LIGHT}")

    for idx in range(len(run_nanorc.data_files)):
        data_file = data_file_checks.DataFile(run_nanorc.data_files[idx])
        all_ok &= data_file_checks.sanity_check(data_file)
        all_ok &= data_file_checks.check_file_attributes(data_file)
        for jdx in range(len(fragment_check_list)):
            all_ok &= data_file_checks.check_fragment_count(
                data_file, fragment_check_list[jdx]
            )
            all_ok &= data_file_checks.check_fragment_sizes(
                data_file, fragment_check_list[jdx]
            )
    assert all_ok, "\N{POLICE CARS REVOLVING LIGHT} One or more raw data file checks failed! \N{POLICE CARS REVOLVING LIGHT}"
