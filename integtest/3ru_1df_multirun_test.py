import pytest
import os
import re
import copy
import psutil
import math
import urllib.request

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.config_file_gen as config_file_gen
import dfmodules.integtest_file_gen as integtest_file_gen

# Values that help determine the running conditions
number_of_data_producers=3
number_of_readout_apps=3
run_duration=20  # seconds
trigger_rate=1 # Hz
data_rate_slowdown_factor=1

# Default values for validation parameters
expected_number_of_data_files=3
check_for_logfile_errors=True
expected_event_count=trigger_rate * run_duration
expected_event_count_tolerance=math.ceil(expected_event_count / 10)
minimum_cpu_count=18
minimum_free_memory_gb=24
wib1_frag_hsi_trig_params={"fragment_type_description": "WIB",
                           "fragment_type": "ProtoWIB",
                           "hdf5_source_subsystem": "Detector_Readout",
                           "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                           "min_size_bytes": 37192, "max_size_bytes": 37656}
wib1_frag_multi_trig_params={"fragment_type_description": "WIB",
                             "fragment_type": "ProtoWIB",
                             "hdf5_source_subsystem": "Detector_Readout",
                             "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                             "min_size_bytes": 72, "max_size_bytes": 54000}
wib2_frag_hsi_trig_params={"fragment_type_description": "WIB",
                           "fragment_type": "WIB",
                           "hdf5_source_subsystem": "Detector_Readout",
                           "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                           "min_size_bytes": 29808, "max_size_bytes": 30280}
wib2_frag_multi_trig_params={"fragment_type_description": "WIB",
                             "fragment_type": "WIB",
                             "hdf5_source_subsystem": "Detector_Readout",
                             "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                             "min_size_bytes": 72, "max_size_bytes": 54000}
wibeth_frag_hsi_trig_params={"fragment_type_description": "WIBEth",
                  "fragment_type": "WIBEth",
                  "hdf5_source_subsystem": "Detector_Readout",
                  "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                  "min_size_bytes": 7272, "max_size_bytes": 14472}
wibeth_frag_multi_trig_params={"fragment_type_description": "WIBEth",
                  "fragment_type": "WIBEth",
                  "hdf5_source_subsystem": "Detector_Readout",
                  "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                  "min_size_bytes": 72, "max_size_bytes": 14472}
triggercandidate_frag_params={"fragment_type_description": "Trigger Candidate",
                              "fragment_type": "Trigger_Candidate",
                              "hdf5_source_subsystem": "Trigger",
                              "expected_fragment_count": 1,
                              "min_size_bytes": 120, "max_size_bytes": 280}
triggeractivity_frag_params={"fragment_type_description": "Trigger Activity",
                              "fragment_type": "Trigger_Activity",
                              "hdf5_source_subsystem": "Trigger",
                              "expected_fragment_count": number_of_readout_apps,
                              "min_size_bytes": 72, "max_size_bytes": 400}
triggertp_frag_params={"fragment_type_description": "Trigger with TPs",
                       "fragment_type": "Trigger_Primitive",
                       "hdf5_source_subsystem": "Trigger",
                       "expected_fragment_count": (2*number_of_readout_apps),
                       "min_size_bytes": 72, "max_size_bytes": 16000}
hsi_frag_params ={"fragment_type_description": "HSI",
                             "fragment_type": "Hardware_Signal",
                             "hdf5_source_subsystem": "HW_Signals_Interface",
                             "expected_fragment_count": 1,
                             "min_size_bytes": 72, "max_size_bytes": 100}
ignored_logfile_problems={"dqm": ["client will not be able to connect to Kafka cluster",
                                  "Unexpected Trigger Decision", "Unexpected Fragment"],
                          "trigger": ["zipped_tpset_q: Unable to push within timeout period"],
                          "ruemu": [r"Trigger Matching result with empty fragment: TS match result on link .+Requestor=\S+fragx_dqm"],
                         }

# Determine if this computer is powerful enough for these tests
sufficient_resources_on_this_computer=True
cpu_count=os.cpu_count()
hostname=os.uname().nodename
mem_obj=psutil.virtual_memory()
free_mem=round((mem_obj.available/(1024*1024*1024)), 2)
total_mem=round((mem_obj.total/(1024*1024*1024)), 2)
print(f"DEBUG: CPU count is {cpu_count}, free and total memory are {free_mem} GB and {total_mem} GB.")
if cpu_count < minimum_cpu_count or free_mem < minimum_free_memory_gb:
    sufficient_resources_on_this_computer=False

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="daqconf_multiru_gen"

# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
dro_map_contents = integtest_file_gen.generate_dromap_contents(number_of_data_producers, number_of_readout_apps)

conf_dict = config_file_gen.get_default_config_dict()
conf_dict["daq_common"]["data_rate_slowdown_factor"] = data_rate_slowdown_factor
conf_dict["readout"]["latency_buffer_size"] = 200000
#conf_dict["readout"]["default_data_file"] = "asset://?label=DuneWIB&subsystem=readout" # DuneWIB
conf_dict["readout"]["default_data_file"] = "asset://?checksum=e96fd6efd3f98a9a3bfaba32975b476e" # WIBEth
conf_dict["detector"]["clock_speed_hz"] = 62500000 # DuneWIB/WIBEth
conf_dict["readout"]["use_fake_cards"] = True
conf_dict["hsi"]["random_trigger_rate_hz"] = trigger_rate

swtpg_conf = copy.deepcopy(conf_dict)
swtpg_conf["readout"]["emulator_mode"] = True
swtpg_conf["readout"]["enable_tpg"] = True
swtpg_conf["readout"]["tpg_threshold"] = 500
swtpg_conf["readout"]["tpg_algorithm"] = "SimpleThreshold"
swtpg_conf["readout"]["default_data_file"] = "asset://?checksum=dd156b4895f1b06a06b6ff38e37bd798" # WIBEth All Zeros
swtpg_conf["trigger"]["trigger_activity_config"] = {"prescale": 25}
swtpg_conf["trigger"]["mlt_merge_overlapping_tcs"] = False
swtpg_conf["dataflow"]["token_count"] = max(10, 3*number_of_data_producers*number_of_readout_apps)

dqm_conf = copy.deepcopy(conf_dict)
dqm_conf["dqm"]["enable_dqm"] = True

confgen_arguments={"WIBEth_System": conf_dict,
                   "Software_TPG_System": swtpg_conf,
                   "DQM_System": dqm_conf,
                  }

# The commands to run in nanorc, as a list
if sufficient_resources_on_this_computer:
    nanorc_command_list="integtest-partition boot conf".split()
    nanorc_command_list+="start_run --wait 1 101 wait ".split() + [str(run_duration)] + "stop_run          wait 2".split()
    nanorc_command_list+="start_run --wait 2 102 wait ".split() + [str(run_duration)] + "stop_run --wait 1 wait 2".split()
    nanorc_command_list+="start_run          103 wait ".split() + [str(run_duration)] + "stop_run --wait 2 wait 2".split()
    nanorc_command_list+="scrap terminate".split()
else:
    nanorc_command_list=["integtest-partition", "boot", "terminate"]

# The tests themselves

def test_nanorc_success(run_nanorc):
    if not sufficient_resources_on_this_computer:
        pytest.skip(f"This computer ({hostname}) does not have enough resources to run this test.")

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
    if not sufficient_resources_on_this_computer:
        pytest.skip(f"This computer ({hostname}) does not have enough resources to run this test.")

    if check_for_logfile_errors:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(run_nanorc.log_files, True, True, ignored_logfile_problems)

def test_data_files(run_nanorc):
    if not sufficient_resources_on_this_computer:
        print(f"This computer ({hostname}) does not have enough resources to run this test.")
        print(f"    (CPU count is {cpu_count}, free and total memory are {free_mem} GB and {total_mem} GB.)")
        print(f"    (Minimum CPU count is {minimum_cpu_count} and minimum free memory is {minimum_free_memory_gb} GB.)")
        pytest.skip(f"This computer ({hostname}) does not have enough resources to run this test.")

    local_expected_event_count=expected_event_count
    local_event_count_tolerance=expected_event_count_tolerance
    fragment_check_list=[triggercandidate_frag_params, hsi_frag_params]
    if "enable_tpg" in run_nanorc.confgen_config["readout"].keys() and run_nanorc.confgen_config["readout"]["enable_tpg"]:
        local_expected_event_count+=(250*number_of_data_producers*number_of_readout_apps*run_duration/100)
        local_event_count_tolerance+=(10*number_of_data_producers*number_of_readout_apps*run_duration/100)
        #fragment_check_list.append(wib1_frag_multi_trig_params) # ProtoWIB
        #fragment_check_list.append(wib2_frag_multi_trig_params) # DuneWIB
        fragment_check_list.append(wibeth_frag_multi_trig_params) # WIBEth
        fragment_check_list.append(triggertp_frag_params)
        fragment_check_list.append(triggeractivity_frag_params)
    else:
        #fragment_check_list.append(wib1_frag_hsi_trig_params) # ProtoWIB
        #fragment_check_list.append(wib2_frag_hsi_trig_params) # DuneWIB
        fragment_check_list.append(wibeth_frag_hsi_trig_params) # WIBEth

    # Run some tests on the output data file
    assert len(run_nanorc.data_files)==expected_number_of_data_files

    for idx in range(len(run_nanorc.data_files)):
        data_file=data_file_checks.DataFile(run_nanorc.data_files[idx])
        assert data_file_checks.sanity_check(data_file)
        assert data_file_checks.check_file_attributes(data_file)
        assert data_file_checks.check_event_count(data_file, local_expected_event_count, local_event_count_tolerance)
        for jdx in range(len(fragment_check_list)):
            assert data_file_checks.check_fragment_count(data_file, fragment_check_list[jdx])
            assert data_file_checks.check_fragment_sizes(data_file, fragment_check_list[jdx])
