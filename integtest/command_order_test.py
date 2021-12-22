import pytest

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks

# Values that help determine the running conditions
number_of_data_producers=2

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
"boot boot".split(),
"boot conf".split(),
"boot start 101".split(),
"boot resume".split(),
"boot pause".split(),
"boot stop".split(),
"boot scrap".split(),

# Only conf after init
"boot init boot".split(),
"boot init init".split(),
"boot init start 102".split(),
"boot init resume".split(),
"boot init pause".split(),
"boot init stop".split(),
"boot init scrap".split(),

# Only start and scrap after conf
"boot init conf boot".split(),
"boot init conf init".split(),
"boot init conf conf".split(),
"boot init conf resume".split(),
"boot init conf pause".split(),
"boot init conf stop".split(),

# Only resume pause stop after start
"boot init conf start 103 boot".split(),
"boot init conf start 104 init".split(),
"boot init conf start 105 conf".split(),
"boot init conf start 106 start 122".split(),
"boot init conf start 107 scrap".split(),

# After stop is same as conf
"boot init conf start 108 stop boot".split(),
"boot init conf start 109 stop init".split(),
"boot init conf start 110 stop conf".split(),
"boot init conf start 111 stop resume".split(),
"boot init conf start 112 stop pause".split(),
"boot init conf start 113 stop stop".split(),

# After scrap is same as init
"boot init conf start 114 stop scrap boot".split(),
"boot init conf start 115 stop scrap init".split(),
"boot init conf start 116 stop scrap start 121".split(),
"boot init conf start 117 stop scrap resume".split(),
"boot init conf start 118 stop scrap pause".split(),
"boot init conf start 119 stop scrap stop".split(),
"boot init conf start 120 stop scrap scrap".split()

]

# The tests themselves
def test_result(run_nanorc):
    assert run_nanorc.completed_process.returncode!=0
