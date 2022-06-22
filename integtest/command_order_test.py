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
["test-partition", "init"],
["test-partition", "conf"],
["test-partition", "start", "100"],
["test-partition", "resume"],
["test-partition", "pause"],
["test-partition", "stop"],
["test-partition", "scrap"],

# Only init after boot
"test-partition boot boot".split(),
"test-partition boot conf".split(),
"test-partition boot start 101".split(),
"test-partition boot resume".split(),
"test-partition boot pause".split(),
"test-partition boot stop".split(),
"test-partition boot scrap".split(),

# Only conf after init
"test-partition boot init boot".split(),
"test-partition boot init init".split(),
"test-partition boot init start 102".split(),
"test-partition boot init resume".split(),
"test-partition boot init pause".split(),
"test-partition boot init stop".split(),
"test-partition boot init scrap".split(),

# Only start and scrap after conf
"test-partition boot init conf boot".split(),
"test-partition boot init conf init".split(),
"test-partition boot init conf conf".split(),
"test-partition boot init conf resume".split(),
"test-partition boot init conf pause".split(),
"test-partition boot init conf stop".split(),

# Only resume pause stop after start
"test-partition boot init conf start 103 boot".split(),
"test-partition boot init conf start 104 init".split(),
"test-partition boot init conf start 105 conf".split(),
"test-partition boot init conf start 106 start 122".split(),
"test-partition boot init conf start 107 scrap".split(),

# After stop is same as conf
"test-partition boot init conf start 108 stop boot".split(),
"test-partition boot init conf start 109 stop init".split(),
"test-partition boot init conf start 110 stop conf".split(),
"test-partition boot init conf start 111 stop resume".split(),
"test-partition boot init conf start 112 stop pause".split(),
"test-partition boot init conf start 113 stop stop".split(),

# After scrap is same as init
"test-partition boot init conf start 114 stop scrap boot".split(),
"test-partition boot init conf start 115 stop scrap init".split(),
"test-partition boot init conf start 116 stop scrap start 121".split(),
"test-partition boot init conf start 117 stop scrap resume".split(),
"test-partition boot init conf start 118 stop scrap pause".split(),
"test-partition boot init conf start 119 stop scrap stop".split(),
"test-partition boot init conf start 120 stop scrap scrap".split(),

# Test valid command after invalid (should still fail)
"test-partition boot boot init conf start 121 stop scrap".split(),
"test-partition boot init stop conf start 122 stop scrap".split(),
"test-partition boot init conf stop start 123 stop scrap".split(),
"test-partition boot init conf start 124 conf stop scrap".split(),
"test-partition boot init conf start 125 stop init scrap".split(),
"test-partition boot init conf start 125 stop scrap boot conf".split(),
"test-partition boot init stop start 126 stop scrap boot conf".split()
]

# The tests themselves
def test_result(run_nanorc):
    assert run_nanorc.completed_process.returncode==30 # InvalidTransition
