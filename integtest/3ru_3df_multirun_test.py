import pytest
import os
import re
import copy
import math
import urllib.request

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.config_file_gen as config_file_gen
import dfmodules.integtest_file_gen as integtest_file_gen

# Values that help determine the running conditions
number_of_data_producers=2
number_of_readout_apps=3
number_of_dataflow_apps=3
trigger_rate=3.0 # Hz
run_duration=20  # seconds
data_rate_slowdown_factor=10

# Default values for validation parameters
expected_number_of_data_files=3*number_of_dataflow_apps
check_for_logfile_errors=True
expected_event_count=run_duration*trigger_rate/number_of_dataflow_apps
expected_event_count_tolerance=expected_event_count/10
wib1_frag_hsi_trig_params={"fragment_type_description": "WIB", 
                           "fragment_type": "ProtoWIB",
                           "hdf5_source_subsystem": "Detector_Readout",
                           "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                           "min_size_bytes": 37192, "max_size_bytes": 37192}
wib1_frag_multi_trig_params={"fragment_type_description": "WIB",
                             "fragment_type": "ProtoWIB",
                             "hdf5_source_subsystem": "Detector_Readout",
                             "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                             "min_size_bytes": 72, "max_size_bytes": 54000}
triggercandidate_frag_params={"fragment_type_description": "Trigger Candidate",
                              "fragment_type": "Trigger_Candidate",
                              "hdf5_source_subsystem": "Trigger",
                              "expected_fragment_count": 1,
                              "min_size_bytes": 72, "max_size_bytes": 280}
triggeractivity_frag_params={"fragment_type_description": "Trigger Activity",
                              "fragment_type": "Trigger_Activity",
                              "hdf5_source_subsystem": "Trigger",
                              "expected_fragment_count": number_of_readout_apps,
                              "min_size_bytes": 72, "max_size_bytes": 216}
triggertp_frag_params={"fragment_type_description": "Trigger with TPs",
                       "fragment_type": "Trigger_Primitive",
                       "hdf5_source_subsystem": "Trigger",
                       "expected_fragment_count": ((number_of_data_producers*number_of_readout_apps)),
                       "min_size_bytes": 72, "max_size_bytes": 16000}
hsi_frag_params ={"fragment_type_description": "HSI",
                             "fragment_type": "Hardware_Signal",
                             "hdf5_source_subsystem": "HW_Signals_Interface",
                             "expected_fragment_count": 1,
                             "min_size_bytes": 72, "max_size_bytes": 96}
ignored_logfile_problems={"dqm": ["client will not be able to connect to Kafka cluster"]}

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="daqconf_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
hardware_map_contents = integtest_file_gen.generate_hwmap_file(number_of_data_producers, number_of_readout_apps)

conf_dict = config_file_gen.get_default_config_dict()
try:
  urllib.request.urlopen('http://localhost:5000').status
  conf_dict["boot"]["use_connectivity_service"] = True
except:
  conf_dict["boot"]["use_connectivity_service"] = False
conf_dict["readout"]["data_rate_slowdown_factor"] = data_rate_slowdown_factor
conf_dict["readout"]["latency_buffer_size"] = 200000
conf_dict["readout"]["default_data_file"] = "asset://?label=ProtoWIB"
conf_dict["trigger"]["trigger_rate_hz"] = trigger_rate

conf_dict["dataflow"]["apps"] = [] # Remove preconfigured dataflow0 app
for df_app in range(number_of_dataflow_apps):
    dfapp_conf = {}
    dfapp_conf["app_name"] = f"dataflow{df_app}"
    conf_dict["dataflow"]["apps"].append(dfapp_conf)

swtpg_conf = copy.deepcopy(conf_dict)
swtpg_conf["readout"]["enable_software_tpg"] = True
for df_app in range(number_of_dataflow_apps):
    swtpg_conf["dataflow"]["apps"][df_app]["token_count"] = int(math.ceil(max(10, 3*number_of_data_producers*number_of_readout_apps)/number_of_dataflow_apps))

dqm_conf = copy.deepcopy(conf_dict)
dqm_conf["dqm"]["enable_dqm"] = True

confgen_arguments={"WIB1_System": conf_dict,
                   "Software_TPG_System": swtpg_conf,
                   "DQM_System": dqm_conf,
                  }
# The commands to run in nanorc, as a list
nanorc_command_list="integtest-partition boot conf".split()
nanorc_command_list+="start 101 enable_triggers wait ".split() + [str(run_duration)] + "stop_run wait 2".split()
nanorc_command_list+="start 102 wait 1 enable_triggers wait ".split() + [str(run_duration)] + "disable_triggers wait 1 stop_run".split()
nanorc_command_list+="start_run 103 wait ".split() + [str(run_duration)] + "disable_triggers wait 1 drain_dataflow wait 1 stop_trigger_sources wait 1 stop wait 2".split()
nanorc_command_list+="shutdown".split()

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
        assert log_file_checks.logs_are_error_free(run_nanorc.log_files, True, True, ignored_logfile_problems)

def test_data_files(run_nanorc):
    local_expected_event_count=expected_event_count
    local_event_count_tolerance=expected_event_count_tolerance
    low_number_of_files=expected_number_of_data_files
    high_number_of_files=expected_number_of_data_files
    fragment_check_list=[triggercandidate_frag_params, hsi_frag_params]
    if "enable_software_tpg" in run_nanorc.confgen_config["readout"].keys() and run_nanorc.confgen_config["readout"]["enable_software_tpg"]:
        local_expected_event_count+=(270*number_of_data_producers*number_of_readout_apps*run_duration/(100*number_of_dataflow_apps))
        local_event_count_tolerance+=(10*number_of_data_producers*number_of_readout_apps*run_duration/(100*number_of_dataflow_apps))
        fragment_check_list.append(wib1_frag_multi_trig_params)
        fragment_check_list.append(triggertp_frag_params)
        fragment_check_list.append(triggeractivity_frag_params)
    else:
        low_number_of_files-=number_of_dataflow_apps
        if low_number_of_files < 1:
            low_number_of_files=1
        fragment_check_list.append(wib1_frag_hsi_trig_params)

    # Run some tests on the output data file
    assert len(run_nanorc.data_files)==high_number_of_files or len(run_nanorc.data_files)==low_number_of_files

    for idx in range(len(run_nanorc.data_files)):
        data_file=data_file_checks.DataFile(run_nanorc.data_files[idx])
        assert data_file_checks.sanity_check(data_file)
        assert data_file_checks.check_file_attributes(data_file)
        assert data_file_checks.check_event_count(data_file, local_expected_event_count, local_event_count_tolerance)
        for jdx in range(len(fragment_check_list)):
            assert data_file_checks.check_fragment_count(data_file, fragment_check_list[jdx])
            assert data_file_checks.check_fragment_sizes(data_file, fragment_check_list[jdx])
