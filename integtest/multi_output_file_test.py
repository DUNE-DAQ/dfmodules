import pytest
import os
import re
import copy
import urllib.request

import integrationtest.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.config_file_gen as config_file_gen
import integrationtest.oks_dro_map_gen as dro_map_gen

pytest_plugins="integrationtest.integrationtest_drunc"

# Values that help determine the running conditions
number_of_data_producers=2
number_of_readout_apps=3
data_rate_slowdown_factor=1

# Default values for validation parameters
expected_number_of_data_files=4
check_for_logfile_errors=True
wib1_frag_hsi_trig_params={"fragment_type_description": "WIB",
                           "fragment_type": "ProtoWIB",
                           "hdf5_source_subsystem": "Detector_Readout",
                           "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                           "min_size_bytes": 37192, "max_size_bytes": 186136}
wib1_frag_multi_trig_params={"fragment_type_description": "WIB",
                             "fragment_type": "ProtoWIB",
                             "hdf5_source_subsystem": "Detector_Readout",
                             "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                             "min_size_bytes": 72, "max_size_bytes": 270000}
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
                  "min_size_bytes": 187272, "max_size_bytes": 194472}
wibeth_frag_multi_trig_params={"fragment_type_description": "WIBEth",
                  "fragment_type": "WIBEth",
                  "hdf5_source_subsystem": "Detector_Readout",
                  "expected_fragment_count": (number_of_data_producers*number_of_readout_apps),
                  "min_size_bytes": 72, "max_size_bytes": 194472}
triggercandidate_frag_params={"fragment_type_description": "Trigger Candidate",
                              "fragment_type": "Trigger_Candidate",
                              "hdf5_source_subsystem": "Trigger",
                              "expected_fragment_count": 1,
                              "min_size_bytes": 72, "max_size_bytes": 280}
triggeractivity_frag_params={"fragment_type_description": "Trigger Activity",
                              "fragment_type": "Trigger_Activity",
                              "hdf5_source_subsystem": "Trigger",
                              "expected_fragment_count": number_of_readout_apps,
                              "min_size_bytes": 72, "max_size_bytes": 520}
triggertp_frag_params={"fragment_type_description": "Trigger with TPs",
                       "fragment_type": "Trigger_Primitive",
                       "hdf5_source_subsystem": "Trigger",
                       "expected_fragment_count": (2*number_of_readout_apps),
                       "min_size_bytes": 72, "max_size_bytes": 16000}
hsi_frag_params ={"fragment_type_description": "HSI",
                             "fragment_type": "Hardware_Signal",
                             "hdf5_source_subsystem": "HW_Signals_Interface",
                             "expected_fragment_count": 1,
                             "min_size_bytes": 72, "max_size_bytes": 128}
ignored_logfile_problems={"trigger": ["zipped_tpset_q: Unable to push within timeout period"]}

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="fddaqconf_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
dro_map_contents = dro_map_gen.generate_dromap_contents(number_of_data_producers, number_of_readout_apps)

conf_dict = config_file_gen.get_default_config_dict()
conf_dict["daq_common"]["data_rate_slowdown_factor"] = data_rate_slowdown_factor
conf_dict["readout"]["latency_buffer_size"] = 200000
conf_dict["detector"]["op_env"] = "integtest"
#conf_dict["readout"]["default_data_file"] = "asset://?label=DuneWIB&subsystem=readout" # DuneWIB
conf_dict["readout"]["default_data_file"] = "asset://?checksum=e96fd6efd3f98a9a3bfaba32975b476e" # WIBEth
conf_dict["detector"]["clock_speed_hz"] = 62500000 # DuneWIB/WIBEth
conf_dict["readout"]["use_fake_cards"] = True
conf_dict["readout"]["emulated_data_times_start_with_now"] = True
conf_dict["hsi"]["random_trigger_rate_hz"] = 10
conf_dict["trigger"]["trigger_window_before_ticks"] = 52000
conf_dict["trigger"]["trigger_window_after_ticks"] = 1000
conf_dict["dataflow"]["apps"][0]["max_file_size"] = 1074000000

swtpg_conf = copy.deepcopy(conf_dict)
swtpg_conf["readout"]["generate_periodic_adc_pattern"] = True
swtpg_conf["readout"]["emulated_TP_rate_per_ch"] = 1
swtpg_conf["readout"]["enable_tpg"] = True
swtpg_conf["readout"]["tpg_threshold"] = 500
swtpg_conf["readout"]["tpg_algorithm"] = "SimpleThreshold"
swtpg_conf["readout"]["default_data_file"] = "asset://?checksum=dd156b4895f1b06a06b6ff38e37bd798" # WIBEth All Zeros
swtpg_conf["trigger"]["trigger_activity_plugin"] = ["TriggerActivityMakerPrescalePlugin"]
swtpg_conf["trigger"]["trigger_activity_config"] = [ {"prescale": 300} ]
swtpg_conf["trigger"]["trigger_candidate_plugin"] = ["TriggerCandidateMakerPrescalePlugin"]
swtpg_conf["trigger"]["trigger_candidate_config"] = [ {"prescale": 100} ]
swtpg_conf["trigger"]["mlt_merge_overlapping_tcs"] = False
swtpg_conf["dataflow"]["token_count"] = max(10, 6*number_of_data_producers*number_of_readout_apps)

multiout_conf = copy.deepcopy(conf_dict)
multiout_conf["dataflow"]["apps"][0]["output_paths"] = [".", "."]
multiout_conf["dataflow"]["apps"][0]["max_file_size"] = 4*1024*1024*1024

multiout_tpg_conf = copy.deepcopy(multiout_conf)
multiout_tpg_conf["readout"]["generate_periodic_adc_pattern"] = True
multiout_tpg_conf["readout"]["emulated_TP_rate_per_ch"] = 1
multiout_tpg_conf["readout"]["enable_tpg"] = True
multiout_tpg_conf["readout"]["tpg_threshold"] = 500
multiout_tpg_conf["readout"]["tpg_algorithm"] = "SimpleThreshold"
multiout_tpg_conf["readout"]["default_data_file"] = "asset://?checksum=dd156b4895f1b06a06b6ff38e37bd798" # WIBEth All Zeros
multiout_tpg_conf["trigger"]["trigger_activity_plugin"] = ["TriggerActivityMakerPrescalePlugin"]
multiout_tpg_conf["trigger"]["trigger_activity_config"] = [ {"prescale": 300} ]
multiout_tpg_conf["trigger"]["trigger_candidate_plugin"] = ["TriggerCandidateMakerPrescalePlugin"]
multiout_tpg_conf["trigger"]["trigger_candidate_config"] = [ {"prescale": 100} ]
multiout_tpg_conf["trigger"]["mlt_merge_overlapping_tcs"] = False
multiout_tpg_conf["dataflow"]["token_count"] = max(10, 6*number_of_data_producers*number_of_readout_apps)

confgen_arguments={"WIBEth_System (Rollover files)": conf_dict,
                   "Software_TPG_System (Rollover files)": swtpg_conf,
                   "WIBEth_System (Multiple outputs)": multiout_conf,
                   "Software_TPG_System (Multiple outputs)": multiout_tpg_conf
                  }

# The commands to run in nanorc, as a list
nanorc_command_list="boot conf start_run 101 wait 180 disable_triggers wait 2 stop_run wait 21 start_run 102 wait 120 disable_triggers wait 2 stop_run wait 21 scrap terminate".split()

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
    local_file_count=expected_number_of_data_files
    fragment_check_list=[triggercandidate_frag_params, hsi_frag_params]
    if "enable_tpg" in run_nanorc.confgen_config["readout"].keys() and run_nanorc.confgen_config["readout"]["enable_tpg"]:
        local_file_count=4  # 5
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
    assert len(run_nanorc.data_files)==local_file_count

    for idx in range(len(run_nanorc.data_files)):
        data_file=data_file_checks.DataFile(run_nanorc.data_files[idx])
        assert data_file_checks.sanity_check(data_file)
        assert data_file_checks.check_file_attributes(data_file)
        for jdx in range(len(fragment_check_list)):
            assert data_file_checks.check_fragment_count(data_file, fragment_check_list[jdx])
            assert data_file_checks.check_fragment_sizes(data_file, fragment_check_list[jdx])

def test_cleanup(run_nanorc):
    pathlist_string=""
    filelist_string=""
    for data_file in run_nanorc.data_files:
        filelist_string += " " + str(data_file)
        if str(data_file.parent) not in pathlist_string:
            pathlist_string += " " + str(data_file.parent)

    if pathlist_string and filelist_string:
        print("============================================")
        print("Listing the hdf5 files before deleting them:")
        print("============================================")

        os.system(f"df -h {pathlist_string}")
        print("--------------------")
        os.system(f"ls -alF {filelist_string}");

        for data_file in run_nanorc.data_files:
            data_file.unlink()

        print("--------------------")
        os.system(f"df -h {pathlist_string}")
        print("============================================")
