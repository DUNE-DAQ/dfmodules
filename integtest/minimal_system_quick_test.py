import pytest

import dfmodules.data_file_checks as data_file_checks
import dfmodules.integtest_file_gen as integtest_file_gen
import integrationtest.log_file_checks as log_file_checks
import integrationtest.config_file_gen as config_file_gen

# Values that help determine the running conditions
number_of_data_producers=2
data_rate_slowdown_factor=10
run_duration=20  # seconds

# Default values for validation parameters
expected_number_of_data_files=1
check_for_logfile_errors=True
expected_event_count=run_duration
expected_event_count_tolerance=2
wib1_frag_hsi_trig_params={"fragment_type_description": "WIB", 
                           "fragment_type": "ProtoWIB",
                           "hdf5_source_subsystem": "Detector_Readout",
                           "expected_fragment_count": number_of_data_producers,
                           "min_size_bytes": 37200, "max_size_bytes": 37200}
triggercandidate_frag_params={"fragment_type_description": "Trigger Candidate",
                              "fragment_type": "Trigger_Candidate",
                              "hdf5_source_subsystem": "Trigger",
                              "expected_fragment_count": 1,
                              "min_size_bytes": 130, "max_size_bytes": 150}

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="daqconf_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)

hardware_map_contents = integtest_file_gen.generate_hwmap_file( number_of_data_producers)

conf_dict = config_file_gen.get_default_config_dict()
conf_dict["daqconf"]["op_env"] = "integtest"
conf_dict["readout"]["data_rate_slowdown_factor"] = data_rate_slowdown_factor

confgen_arguments={"MinimalSystem": conf_dict}
# The commands to run in nanorc, as a list
nanorc_command_list="integtest-partition boot conf start 101 wait 1 enable_triggers wait ".split() + [str(run_duration)] + "disable_triggers wait 2 stop_run wait 2 scrap terminate".split()

# The tests themselves

def test_nanorc_success(run_nanorc):
    # Check that nanorc completed correctly
    assert run_nanorc.completed_process.returncode==0

def test_log_files(run_nanorc):
    if check_for_logfile_errors:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(run_nanorc.log_files)

def test_data_files(run_nanorc):
    # Run some tests on the output data file
    assert len(run_nanorc.data_files)==expected_number_of_data_files

    fragment_check_list=[]
    fragment_check_list.append(wib1_frag_hsi_trig_params)
    fragment_check_list.append(triggercandidate_frag_params)

    for idx in range(len(run_nanorc.data_files)):
        data_file=data_file_checks.DataFile(run_nanorc.data_files[idx])
        assert data_file_checks.sanity_check(data_file)
        assert data_file_checks.check_file_attributes(data_file)
        assert data_file_checks.check_event_count(data_file, expected_event_count, expected_event_count_tolerance)
        for jdx in range(len(fragment_check_list)):
            assert data_file_checks.check_fragment_count(data_file, fragment_check_list[jdx])
            assert data_file_checks.check_fragment_sizes(data_file, fragment_check_list[jdx])
