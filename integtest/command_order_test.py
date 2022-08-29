import pytest
import copy

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.config_file_gen as config_file_gen
import dfmodules.integtest_file_gen as integtest_file_gen

# Values that help determine the running conditions
number_of_data_producers=2
data_rate_slowdown_factor=10

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="daqconf_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
hardware_map_contents = integtest_file_gen.generate_hwmap_file(number_of_data_producers)

conf_dict = config_file_gen.get_default_config_dict()
conf_dict["readout"]["data_rate_slowdown_factor"] = data_rate_slowdown_factor
conf_dict["readout"]["latency_buffer_size"] = 200000
conf_dict["readout"]["enable_software_tpg"] = True
conf_dict["dqm"]["enable_dqm"] = True

confgen_arguments={"Test_System": conf_dict
                  }
# The commands to run in nanorc, as a list
nanorc_command_list=[

# No commands are valid before boot
["integtest-partition", "init"],
["integtest-partition", "conf"],
["integtest-partition", "start", "100"],
["integtest-partition", "resume"],
["integtest-partition", "disable_triggers"],
["integtest-partition", "stop"],
["integtest-partition", "scrap"],

# Only init after boot
"integtest-partition boot boot".split(),
"integtest-partition boot conf".split(),
"integtest-partition boot start 101".split(),
"integtest-partition boot resume".split(),
"integtest-partition boot disable_triggers".split(),
"integtest-partition boot stop".split(),
"integtest-partition boot scrap".split(),

# Only conf after init
"integtest-partition boot init boot".split(),
"integtest-partition boot init init".split(),
"integtest-partition boot init start 102".split(),
"integtest-partition boot init resume".split(),
"integtest-partition boot init disable_triggers".split(),
"integtest-partition boot init stop".split(),
"integtest-partition boot init scrap".split(),

# Only start and scrap after conf
"integtest-partition boot init conf boot".split(),
"integtest-partition boot init conf init".split(),
"integtest-partition boot init conf conf".split(),
"integtest-partition boot init conf resume".split(),
"integtest-partition boot init conf disable_triggers".split(),
"integtest-partition boot init conf stop".split(),

# Only resume disable_triggers stop after start
"integtest-partition boot init conf start 103 boot".split(),
"integtest-partition boot init conf start 104 init".split(),
"integtest-partition boot init conf start 105 conf".split(),
"integtest-partition boot init conf start 106 start 122".split(),
"integtest-partition boot init conf start 107 scrap".split(),

# After stop is same as conf
"integtest-partition boot init conf start 108 stop boot".split(),
"integtest-partition boot init conf start 109 stop init".split(),
"integtest-partition boot init conf start 110 stop conf".split(),
"integtest-partition boot init conf start 111 stop resume".split(),
"integtest-partition boot init conf start 112 stop disable_triggers".split(),
"integtest-partition boot init conf start 113 stop stop".split(),

# After scrap is same as init
"integtest-partition boot init conf start 114 stop scrap boot".split(),
"integtest-partition boot init conf start 115 stop scrap init".split(),
"integtest-partition boot init conf start 116 stop scrap start 121".split(),
"integtest-partition boot init conf start 117 stop scrap resume".split(),
"integtest-partition boot init conf start 118 stop scrap disable_triggers".split(),
"integtest-partition boot init conf start 119 stop scrap stop".split(),
"integtest-partition boot init conf start 120 stop scrap scrap".split(),

# Test valid command after invalid (should still fail)
"integtest-partition boot boot init conf start 121 stop scrap".split(),
"integtest-partition boot init stop conf start 122 stop scrap".split(),
"integtest-partition boot init conf stop start 123 stop scrap".split(),
"integtest-partition boot init conf start 124 conf stop scrap".split(),
"integtest-partition boot init conf start 125 stop init scrap".split(),
"integtest-partition boot init conf start 125 stop scrap boot conf".split(),
"integtest-partition boot init stop start 126 stop scrap boot conf".split()
]

# The tests themselves
def test_result(run_nanorc):
    assert run_nanorc.completed_process.returncode==30 # InvalidTransition
