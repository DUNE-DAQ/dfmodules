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

pytest_plugins = "integrationtest.integrationtest_drunc"

# 02-Jun-2025, KAB: tweak the print() statement default behavior so that it always flushes the output.
import functools
print = functools.partial(print, flush=True)

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
resval = resource_validation.ResourceValidator()
resval.require_cpu_count(45)  # total number of data sources plus 50% more for everything else
resval.require_free_memory_gb(28)  # the maximum amount that we observe being used ('free -h')
resval.require_total_memory_gb(56)  # double what we need; trying to be kind to others
actual_output_path = output_path_parameter
if output_path_parameter == ".":
    actual_output_path = "/tmp"
# The largest data set in this test is four 2.55 GB files.  To handle these four files
# plus a safety factor of three for the last one, we need 6*2.55 = 15.3.
# Note that a safety factor of 3 is configured below, over-riding the default value of 5.
resval.require_free_disk_space_gb(actual_output_path, 16)
# The value of 19 GB for the total disk space is just to reserve some space beyond what this
# test needs for others to use, and to be slightly lower than a typical 20 GB full disk size.
resval.require_total_disk_space_gb(actual_output_path, 19)
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
conf_dict.session = "largerecord"
conf_dict.tpg_enabled = False
conf_dict.n_df_apps = number_of_dataflow_apps

conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="RandomTCMakerConf",
        updates={"trigger_rate_hz": trigger_rate},
    )
)
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

conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="TCReadoutMap",
        updates={
            "time_before": readout_window_time_before,
            "time_after": readout_window_time_after,
        },
    )
)

# Now set the readout window for the over-size case
oversize_conf.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="TCReadoutMap",
        updates={
            "time_before": 2.5 * readout_window_time_before,
            "time_after": readout_window_time_after,
        },
    )
)

confgen_arguments = {
    "TRSize_55PercentOfMaxFileSize": conf_dict,
    "TRSize_125PercentOfMaxFileSize": oversize_conf,
}
# The commands to run in nanorc, as a list
if resval.this_computer_has_sufficient_resources:
    nanorc_command_list = (
        "boot conf wait 5".split()
        + "start --run-number 101 wait 10 enable-triggers wait ".split()
        + [str(run_duration)]
        + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop ".split()
        + "start --run-number 102 wait 10 enable-triggers wait ".split()
        + [str(run_duration)]
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
    all_ok = len(run_nanorc.data_files) == expected_number_of_data_files
    print("") # Clear potential dot from pytest
    if all_ok:
        print(f"\N{WHITE HEAVY CHECK MARK} The correct number of raw data files was found ({expected_number_of_data_files})")
    else:
        print(f"\N{POLICE CARS REVOLVING LIGHT} An incorrect number of raw data files was found, expected {expected_number_of_data_files}, found {len(run_nanorc.data_files)} \N{POLICE CARS REVOLVING LIGHT}")

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


def test_cleanup(run_nanorc):
    if not resval.this_computer_has_sufficient_resources:
        resval_summary_string = resval.get_insufficient_resources_summary()
        pytest.skip(f"{resval_summary_string}")

    pathlist_string = ""
    filelist_string = ""
    for data_file in run_nanorc.data_files:
        filelist_string += " " + str(data_file)
        if str(data_file.parent) not in pathlist_string:
            pathlist_string += " " + str(data_file.parent)

    if pathlist_string and filelist_string:
        print("============================================")
        print("Listing the hdf5 files before deleting them:")
        print("============================================")

        os.system(f"df -h {pathlist_string}")
        print("--------------------")
        os.system(f"ls -alF {filelist_string}")

        for data_file in run_nanorc.data_files:
            data_file.unlink()

        print("--------------------")
        os.system(f"df -h {pathlist_string}")
        print("============================================")
