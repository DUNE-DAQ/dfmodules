import pytest

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks

# Values that help determine the running conditions
number_of_data_producers=2
run_duration=20  # seconds

# Default values for validation parameters
expected_number_of_data_files=2
expected_fragments_per_trigger_record=number_of_data_producers
min_fragment_size_bytes=37200
max_fragment_size_bytes=37200
check_for_logfile_errors=True

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="minidaqapp.nanorc.mdapp_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
confgen_arguments_base=[ "-d", "./frames.bin", "-o", ".", "-s", "10", "-n", str(number_of_data_producers), "-b", "1000", "-a", "1000", "--host-ru", "localhost"]
confgen_arguments=[ confgen_arguments_base, confgen_arguments_base+["--enable-software-tpg"], confgen_arguments_base+["--enable-dqm"] ]
# The commands to run in nanorc, as a list
nanorc_command_list="boot init conf".split()
nanorc_command_list+="start --disable-data-storage 101 wait ".split() + [str(run_duration)] + "stop --stop-wait 2 wait 2".split()
nanorc_command_list+="start                        102 wait ".split() + [str(run_duration)] + "stop --stop-wait 2 wait 2".split()
nanorc_command_list+="start --disable-data-storage 103 wait ".split() + [str(run_duration)] + "pause wait 2 stop  wait 2".split()
nanorc_command_list+="start                        104 wait ".split() + [str(run_duration)] + "pause wait 2 stop  wait 2".split()
nanorc_command_list+="scrap terminate".split()

# The tests themselves

def test_nanorc_success(run_nanorc):
    # Check that nanorc completed correctly
    assert run_nanorc.completed_process.returncode==0

def test_log_files(run_nanorc):
    local_check_flag=check_for_logfile_errors
    if "--enable-dqm" in run_nanorc.confgen_arguments:
        local_check_flag=False

    if local_check_flag:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(run_nanorc.log_files)

def test_data_file(run_nanorc):
    local_expected_frags=expected_fragments_per_trigger_record
    local_min_frag_size=min_fragment_size_bytes
    local_max_frag_size=max_fragment_size_bytes
    if "--enable-software-tpg" in run_nanorc.confgen_arguments:
        local_expected_frags*=2
        local_min_frag_size=80

    # Run some tests on the output data file
    assert len(run_nanorc.data_files)==expected_number_of_data_files

    for idx in range(len(run_nanorc.data_files)):
        data_file=data_file_checks.DataFile(run_nanorc.data_files[idx])
        assert data_file_checks.sanity_check(data_file)
        assert data_file_checks.check_link_presence(data_file, n_links=local_expected_frags)
        assert data_file_checks.check_fragment_sizes(data_file, min_frag_size=local_min_frag_size, max_frag_size=local_max_frag_size)
