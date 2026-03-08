import pytest
import os
import re
import urllib.request

import integrationtest.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.data_classes as data_classes
import integrationtest.resource_validation as resource_validation

pytest_plugins = "integrationtest.integrationtest_drunc"

# 02-Jun-2025, KAB: tweak the print() statement default behavior so that it always flushes the output.
import functools
print = functools.partial(print, flush=True)

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
resval = resource_validation.ResourceValidator()
resval.require_cpu_count(45)  # total number of data sources plus 50% more for everything else
resval.require_free_memory_gb(35)  # the maximum amount that we observe being used ('free -h')
resval.require_total_memory_gb(70)  # double what we need; trying to be kind to others
actual_output_path = output_path_parameter
if output_path_parameter == ".":
    actual_output_path = "/tmp"
resval.require_free_disk_space_gb(actual_output_path, minimum_free_disk_space_gb)
resval_debug_string = resval.get_debug_string()
print(f"{resval_debug_string}")

# We simulate a nearly-full output disk by setting the free-space-safety-factor
# that the data writer uses to a custom value, based on the free space on disk.
# The size of each TriggerRecord in this test is tuned to be about 1 GB, so if
# the free-space-safety-factor is calculated to be 10, then it will appear that
# the disk is full when there is still ~< 10 GB of free space.  And, having a
# 1 GB size for the TRs means that we will write approximately
# desired_free_disk_space_gb TriggerRecords before appearing to run out of space.
free_space_safety_factor = int(resval.free_disk_space_gb - desired_size_of_output_disk_gb)

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

object_databases = ["config/daqsystemtest/integrationtest-objects.data.xml"]

conf_dict = data_classes.drunc_config()
conf_dict.dro_map_config.n_streams = number_of_data_producers
conf_dict.dro_map_config.n_apps = number_of_readout_apps
conf_dict.op_env = "integtest"
conf_dict.config_session_name= "insufficient"
conf_dict.tpg_enabled = False
conf_dict.n_df_apps = number_of_dataflow_apps
conf_dict.fake_hsi_enabled = False

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
# The commands to run in nanorc, as a list
if resval.this_computer_has_sufficient_resources:
    nanorc_command_list = (
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
            run_nanorc.log_files,
            True,
            True,
            ignored_logfile_problems,
            required_logfile_problems,
        )


def test_data_files(run_nanorc):
    if not resval.this_computer_has_sufficient_resources:
        resval_summary_string = resval.get_insufficient_resources_summary()
        pytest.skip(f"{resval_summary_string}")

    local_expected_event_count = expected_event_count
    local_event_count_tolerance = expected_event_count_tolerance
    fragment_check_list = [triggercandidate_frag_params]
    # fragment_check_list.append(wib1_frag_hsi_trig_params)
    # fragment_check_list.append(wib2_frag_hsi_trig_params) # DuneWIB
    fragment_check_list.append(wibeth_frag_hsi_trig_params)  # WIBEth

    # Run some tests on the output data file
    all_ok = len(run_nanorc.data_files) == expected_number_of_data_files or len(run_nanorc.data_files) == (expected_number_of_data_files+1)
    print("") # Clear potential dot from pytest
    if all_ok:
        print(f"\N{WHITE HEAVY CHECK MARK} An acceptable number of raw data files was found ({len(run_nanorc.data_files)} in {expected_number_of_data_files}..{expected_number_of_data_files+1})")
    else:
        print(f"\N{POLICE CARS REVOLVING LIGHT} An incorrect number of raw data files was found, expected {expected_number_of_data_files}..{expected_number_of_data_files+1}, found {len(run_nanorc.data_files)} \N{POLICE CARS REVOLVING LIGHT}")

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
