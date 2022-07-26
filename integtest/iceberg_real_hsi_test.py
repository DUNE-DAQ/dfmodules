import pytest
import os
import re
import psutil

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks

# Values that help determine the running conditions
number_of_data_producers=2
number_of_readout_apps=2
number_of_dataflow_apps=1
base_trigger_rate=1.0 # Hz
trigger_rate_factor=3.5
run_duration=20  # seconds

# Default values for validation parameters
expected_number_of_data_files=3*number_of_dataflow_apps
check_for_logfile_errors=True
expected_event_count=run_duration*base_trigger_rate/number_of_dataflow_apps
expected_event_count_tolerance=expected_event_count/10
wib2_frag_hsi_trig_params={"fragment_type_description": "WIB2",
                           "hdf5_detector_group": "TPC", "hdf5_region_prefix": "APA",
                           "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                           "min_size_bytes": 29816, "max_size_bytes": 29816}
triggercandidate_frag_params={"fragment_type_description": "Trigger Candidate",
                              "hdf5_detector_group": "Trigger", "hdf5_region_prefix": "Region",
                              "expected_fragment_count": 1,
                              "min_size_bytes": 130, "max_size_bytes": 150}

# Determine if the conditions are right for these tests
we_are_running_on_an_iceberg_computer=False
hostname=os.uname().nodename
#if "iceberg01" in hostname or "protodune-daq02" in hostname:
if "iceberg01" in hostname:
    we_are_running_on_an_iceberg_computer=True
the_global_timing_partition_is_running=False
username=os.environ.get('USER')
for proc in psutil.process_iter(['pid', 'name', 'username']):
    if proc.username() == username and "nanotimingrc" in proc.name():
        the_global_timing_partition_is_running=True
print(f"DEBUG: hostname is {hostname}, iceberg-computer flag is {we_are_running_on_an_iceberg_computer} and global-timing-running flag is {the_global_timing_partition_is_running}.")

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="daqconf_multiru_gen"

# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
if we_are_running_on_an_iceberg_computer and the_global_timing_partition_is_running:
    confgen_arguments_base=["-o", ".", "-n", str(number_of_data_producers), "-b", "1000", "-a", "1000", "--latency-buffer-size", "200000"] + [ "--host-ru", "localhost" ] * number_of_readout_apps + [ "--host-df", "localhost" ] * number_of_dataflow_apps + ["--use-hsi-hw", "--host-hsi", "iceberg01-priv", "--control-hsi-hw", "--hsi-device-name", "BOREAS_TLU_ICEBERG", "--hsi-source", "1", "--ttcm-s1", "1", "--hsi-re-mask", "1", "--host-timing", "iceberg01-priv", "--control-timing-partition", "--host-tprtc", "iceberg01-priv", "--timing-partition-master-device-name", "BOREAS_TLU_ICEBERG", "--hsi-trigger-type-passthrough", "--use-fake-data-producers", "--frontend-type", "wib2", "--clock-speed-hz", "62500000"]
    confgen_arguments={"Base_Trigger_Rate": confgen_arguments_base+["-t", str(base_trigger_rate)],
                       "Trigger_Rate_with_Factor": confgen_arguments_base+["-t", str(base_trigger_rate*trigger_rate_factor)]
                      }
else:
    confgen_arguments=["-o", ".", "-s", "10"]

# The commands to run in nanorc, as a list
if we_are_running_on_an_iceberg_computer and the_global_timing_partition_is_running:
    nanorc_command_list="integtest-partition boot conf".split()
    nanorc_command_list+="start 101 enable_triggers wait ".split() + [str(run_duration)] + "stop_run wait 2".split()
    nanorc_command_list+="start 102 wait 1 enable_triggers wait ".split() + [str(run_duration)] + "disable_triggers wait 1 stop_run".split()
    nanorc_command_list+="start_run 103 wait ".split() + [str(run_duration)] + "disable_triggers wait 1 drain_dataflow wait 1 stop_trigger_sources wait 1 stop wait 2".split()
    nanorc_command_list+="scrap terminate".split()
else:
    nanorc_command_list=["integtest-partition", "boot", "terminate"]

# Don't require the --frame-file option since we don't need it
frame_file_required=False

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
        assert log_file_checks.logs_are_error_free(run_nanorc.log_files, True, True)

def test_data_files(run_nanorc):
    if not we_are_running_on_an_iceberg_computer:
        print(f"This computer ({hostname}) is not part of the ICEBERG DAQ cluster and therefore can not run this test.")
        return
    if not the_global_timing_partition_is_running:
        print(f"The global timing partition does not appear to be running on this computer ({hostname}).")
        print("    Please check whether it is, and start it, if needed.")
        print("Hints: daqconf_timing_gen --host-thi iceberg01-priv --host-tmc iceberg01-priv --master-device-name BOREAS_TLU_ICEBERG --clock-speed-hz 62500000 timing_partition_config")
        print("       nanotimingrc timing_partition_config ${USER}-timing-partition boot conf wait 1200 scrap terminate")
        return

    fragment_check_list=[]
    fragment_check_list.append(wib2_frag_hsi_trig_params)
    fragment_check_list.append(triggercandidate_frag_params)

    local_expected_event_count=expected_event_count
    local_event_count_tolerance=expected_event_count_tolerance
    current_test=os.environ.get('PYTEST_CURRENT_TEST')
    match_obj = re.search(r"Factor", current_test)
    if match_obj:
        local_expected_event_count*=trigger_rate_factor
        local_event_count_tolerance*=trigger_rate_factor

    # Run some tests on the output data files
    assert len(run_nanorc.data_files)==expected_number_of_data_files

    for idx in range(len(run_nanorc.data_files)):
        data_file=data_file_checks.DataFile(run_nanorc.data_files[idx])
        assert data_file_checks.sanity_check(data_file)
        assert data_file_checks.check_file_attributes(data_file)
        assert data_file_checks.check_event_count(data_file, local_expected_event_count, local_event_count_tolerance)
        for jdx in range(len(fragment_check_list)):
            assert data_file_checks.check_fragment_count(data_file, fragment_check_list[jdx])
            assert data_file_checks.check_fragment_sizes(data_file, fragment_check_list[jdx])

# ### also test the expected trigger bit ###

