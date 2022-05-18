import pytest

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks

# Values that help determine the running conditions
number_of_data_producers=2

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="daqconf_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
confgen_arguments_base=[ "-d", "./frames.bin", "-o", ".", "-s", "10", "-n", str(number_of_data_producers), "-b", "1000", "-a", "1000", "--host-ru", "localhost"]
confgen_arguments=[ confgen_arguments_base, confgen_arguments_base+["--enable-software-tpg"], confgen_arguments_base+["--enable-dqm"] ]
# The commands to run in nanorc, as a list
nanorc_command_list=[

# No commands are valid before boot
["init"],
["conf"],
["start", "100"],
["resume"],
["pause"],
["stop"],
["scrap"],

# Only init after boot
"boot test-partition boot test-partition".split(),
"boot test-partition conf".split(),
"boot test-partition start 101".split(),
"boot test-partition resume".split(),
"boot test-partition pause".split(),
"boot test-partition stop".split(),
"boot test-partition scrap".split(),

# Only conf after init
"boot test-partition init boot test-partition".split(),
"boot test-partition init init".split(),
"boot test-partition init start 102".split(),
"boot test-partition init resume".split(),
"boot test-partition init pause".split(),
"boot test-partition init stop".split(),
"boot test-partition init scrap".split(),

# Only start and scrap after conf
"boot test-partition init conf boot test-partition".split(),
"boot test-partition init conf init".split(),
"boot test-partition init conf conf".split(),
"boot test-partition init conf resume".split(),
"boot test-partition init conf pause".split(),
"boot test-partition init conf stop".split(),

# Only resume pause stop after start
"boot test-partition init conf start 103 boot test-partition".split(),
"boot test-partition init conf start 104 init".split(),
"boot test-partition init conf start 105 conf".split(),
"boot test-partition init conf start 106 start 122".split(),
"boot test-partition init conf start 107 scrap".split(),

# After stop is same as conf
"boot test-partition init conf start 108 stop boot test-partition".split(),
"boot test-partition init conf start 109 stop init".split(),
"boot test-partition init conf start 110 stop conf".split(),
"boot test-partition init conf start 111 stop resume".split(),
"boot test-partition init conf start 112 stop pause".split(),
"boot test-partition init conf start 113 stop stop".split(),

# After scrap is same as init
"boot test-partition init conf start 114 stop scrap boot test-partition".split(),
"boot test-partition init conf start 115 stop scrap init".split(),
"boot test-partition init conf start 116 stop scrap start 121".split(),
"boot test-partition init conf start 117 stop scrap resume".split(),
"boot test-partition init conf start 118 stop scrap pause".split(),
"boot test-partition init conf start 119 stop scrap stop".split(),
"boot test-partition init conf start 120 stop scrap scrap".split(),

# Test valid command after invalid (should still fail)
"boot test-partition boot test-partition init conf start 121 stop scrap".split(),
"boot test-partition init stop conf start 122 stop scrap".split(),
"boot test-partition init conf stop start 123 stop scrap".split(),
"boot test-partition init conf start 124 conf stop scrap".split(),
"boot test-partition init conf start 125 stop init scrap".split(),
"boot test-partition init conf start 125 stop scrap boot test-partition conf".split(),
"boot test-partition init stop start 126 stop scrap boot test-partition conf".split()
]

# The tests themselves
def test_result(run_nanorc):
    assert run_nanorc.completed_process.returncode==30 # InvalidTransition
