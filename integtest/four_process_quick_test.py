import pytest

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks

# Values that help determine the running conditions
number_of_data_producers=2
run_duration=20  # seconds

# Default values for validation parameters
expected_number_of_data_files=1
check_for_logfile_errors=True
min_expected_event_count=run_duration-1
max_expected_event_count=run_duration+2
wib1_frag_hsi_trig_params={"fragment_type_description": "WIB", "hdf5_groups": "TPC/APA000",
                           "element_name_prefix": "Link", "element_number_offset": 0,
                           "expected_fragment_count": number_of_data_producers,
                           "min_size_bytes": 37200, "max_size_bytes": 37200}

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="minidaqapp.nanorc.mdapp_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
confgen_arguments=[ "-d", "./frames.bin", "-o", ".", "-s", "10", "-n", str(number_of_data_producers), "-b", "1000", "-a", "1000", "--host-ru", "localhost"]
# The commands to run in nanorc, as a list
nanorc_command_list="boot init conf start 101 wait 1 resume wait ".split() + [str(run_duration)] + "pause wait 2 stop wait 2 scrap terminate".split()

# The tests themselves

def test_nanorc_success(run_nanorc):
    # Check that nanorc completed correctly
    assert run_nanorc.completed_process.returncode==0

def test_log_files(run_nanorc):
    if check_for_logfile_errors:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(run_nanorc.log_files)

def test_data_file(run_nanorc):
    # Run some tests on the output data file
    assert len(run_nanorc.data_files)==expected_number_of_data_files

    for idx in range(len(run_nanorc.data_files)):
        data_file=data_file_checks.DataFile(run_nanorc.data_files[idx])
        assert data_file_checks.sanity_check(data_file)
        assert data_file_checks.check_event_count(data_file, min_expected_event_count, max_expected_event_count)
        assert data_file_checks.check_fragment_count(data_file, wib1_frag_hsi_trig_params)
        assert data_file_checks.check_fragment_presence(data_file, wib1_frag_hsi_trig_params)
        assert data_file_checks.check_fragment_size2(data_file, wib1_frag_hsi_trig_params)
