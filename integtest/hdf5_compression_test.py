import pytest
import os
import re
import urllib.request

import integrationtest.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.basic_checks as basic_checks
import integrationtest.data_classes as data_classes
import integrationtest.resource_validation as resource_validation
import integrationtest.utility_functions2 as utility_functions
from integrationtest.get_pytest_tmpdir import get_pytest_tmpdir
from integrationtest.verbosity_helper import IntegtestVerbosityLevels

import functools
print = functools.partial(print, flush=True)  # always flush print() output

pytest_plugins = "integrationtest.integrationtest_drunc"

# Values that help determine the running conditions
number_of_data_producers = 2
number_of_readout_apps = 3

# Default values for validation parameters
check_for_logfile_errors = True

daphne_frag_params = {
    "fragment_type_description": "DAPHNE",
    "fragment_type": "DAPHNE",
    "expected_fragment_count": number_of_data_producers * number_of_readout_apps,
    "min_size_bytes": 1936,
    "max_size_bytes": 230000,
#    "frag_sizes_by_TC_type": {"kPrescale": {"min_size_bytes":   1936, "max_size_bytes":  30000},
#                                "kRandom": {"min_size_bytes": 220000, "max_size_bytes": 230000},
#                                "default": {"min_size_bytes":   1936, "max_size_bytes": 230000} }
}
daphne_triggerprimitive_frag_params = {
    "fragment_type_description": "Trigger Primitive",
    "fragment_type": "Trigger_Primitive",
    "expected_fragment_count": number_of_readout_apps,
    "min_size_bytes": 72,
    "max_size_bytes": 230000,
#    "frag_sizes_by_TC_type": {"kPrescale": {"min_size_bytes":     72, "max_size_bytes":  10000},
#                                "kRandom": {"min_size_bytes": 220000, "max_size_bytes": 230000},
#                                "default": {"min_size_bytes":     72, "max_size_bytes": 230000} }
}
daphne_tpset_params = {
    "fragment_type_description": "TP Stream",
    "fragment_type": "Trigger_Primitive",
    "expected_fragment_count": number_of_readout_apps,
    "frag_counts_by_record_ordinal": { "first": {"min_count": 1, "max_count": number_of_readout_apps},
                                        "last": {"min_count": 1, "max_count": number_of_readout_apps},
                                      "default": {"min_count": number_of_readout_apps, "max_count": number_of_readout_apps} },
    "min_size_bytes": 200,
    "max_size_bytes": 210,
    "debug_mask": 0x0,
#    "frag_sizes_by_record_ordinal": {  "first": {"min_size_bytes":    96, "max_size_bytes": 120000},
#                                      "second": {"min_size_bytes":    96, "max_size_bytes": 120000},
#                                        "last": {"min_size_bytes":    96, "max_size_bytes": 120000},
#                                     "default": {"min_size_bytes": 80000, "max_size_bytes": 120000} }
}

# sizes: 128 is for one TC with zero TAs inside it (72+56)
#        208 is for one TC with one TA inside it (72+56+80)
#        264 is for two TCs with one TA in one of them (72+56+80+56)
triggercandidate_frag_params = {
    "fragment_type_description": "Trigger Candidate",
    "fragment_type": "Trigger_Candidate",
    "expected_fragment_count": 1,
    "min_size_bytes": 128,
    "max_size_bytes": 264,
}
# sizes:  72 is for an empty TA fragment
#        632 is for three TAs with one TP in each of them (72+5*(88+24))
triggeractivity_frag_params = {
    "fragment_type_description": "Trigger Activity",
    "fragment_type": "Trigger_Activity",
    "expected_fragment_count": 1,
    "min_size_bytes": 72,
    "max_size_bytes": 632,
}
# sizes:  72 is for an empty TP fragment
#       1032 is for a fragment with 40 TPs in it (72+(40*24))
triggerprimitive_frag_params = {
    "fragment_type_description": "Trigger Primitive",
    "fragment_type": "Trigger_Primitive",
    "expected_fragment_count": number_of_readout_apps,
    "min_size_bytes": 72,
    "max_size_bytes": 1032,
}
# 03-Jul-2025, KAB: changing the default max size from 72 to 100 to handle cases in which there
# was a Random or Prescale trigger along with a coincidental HSI event within the readout window.
hsi_frag_params = {
    "fragment_type_description": "HSI",
    "fragment_type": "Hardware_Signal",
    "expected_fragment_count": 1,
    "min_size_bytes": 72,
    "max_size_bytes": 100,
    "frag_sizes_by_TC_type": {"kTiming": {"min_size_bytes": 100, "max_size_bytes": 100},
                              "default": {"min_size_bytes":  72, "max_size_bytes": 100} }
}
ignored_logfile_problems = {
    "-controller": [
        "Worker with pid \\d+ was terminated due to signal 1",
    ],
    "connectivity-service": [
        "errorlog: -",
    ],
}

# Determine if the conditions are right for these tests
resource_validator = resource_validation.ResourceValidator()
resource_validator.cpu_count_needs(9, 18)  # total number of data sources (6) plus three more for everything else
resource_validator.free_memory_needs(12, 25)  # 20% more than what we observe being used ('free -h')
resource_validator.total_memory_needs()  # no specific request, but it's useful to see how much is available
actual_output_path = get_pytest_tmpdir()
resource_validator.free_disk_space_needs(actual_output_path, 5)  # what we actually use (3) plus margin
resource_validator.total_disk_space_needs(actual_output_path, recommended_total_disk_space=10)  # double what we need


conf_dict = data_classes.integtest_params_for_generated_dunedaq_config()
conf_dict.object_databases = ["config/daqsystemtest/integrationtest-objects.data.xml"]
conf_dict.dro_map_config.n_streams = number_of_data_producers
conf_dict.dro_map_config.n_apps = number_of_readout_apps
conf_dict.op_env = "integtest"
conf_dict.config_session_name= "hdf5compression"
conf_dict.tpg_enabled = True
utility_functions.enable_fake_hsi_trigger(conf_dict, trigger_rate=10.0,
                                          readout_window_before_ticks=120000,
                                          readout_window_after_ticks=1000)
conf_dict.dro_map_config.det_id = 2  # det_id = 2 for kHD_PDS
conf_dict.frame_file = "asset://?checksum=a8990a9eb3a505d4ded62dfdfa9e2681"  # run 36012 DAPHNE data
#conf_dict.frame_file = "file:///home/nfs/biery/dunedaq/12MayFDv5.3.2DevInstrUpdate/sourcecode/dfmodules/integtest/np02vdcoldbox_run035227_sample_hd_pds.bin"

conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="TAMakerPrescaleAlgorithm",
        obj_id="dummy-ta-maker",
        updates={"prescale": 2000},
    )
)

conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="DataStoreConf",
        obj_id="default",
        updates={"max_file_size": 400000000},
    )
)
conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="DataStoreConf",
        obj_id="default_tp_store_conf",
        updates={"max_file_size": 170000000},
    )
)
conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="DataStoreConf",
        obj_id="default",
        updates={"compression_level": 5},
    )
)
conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="DataStoreConf",
        obj_id="default_tp_store_conf",
        updates={"compression_level": 1},
    )
)

conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="DFOConf", updates={"busy_threshold": 16, "free_threshold": 12}
    )
)
conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="TCDataProcessor",
        obj_id="def-tc-processor",
        updates={"merge_overlapping_tcs": 0},
    )
)
conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="LatencyBuffer", updates={"size": 200000}
    )
)
# 03-Jun-2025, KAB
# TP rate is ~67 kHz with the run 36012 replay data file.
# 67 kHz * 2 DataLinkHandlers per RU * safety factor of 3 seconds ~= 400,000
# However, this doesn't always seem to be large enough, so we'll use 1e6 for now.
conf_dict.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="QueueDescriptor",
        obj_id="tp-input",
        updates={"capacity": 1000000},
    )
)


confgen_arguments = {
    "DAPHNE_TPG_System": conf_dict,
}

# The commands to run in dunerc, as a list
dunerc_command_list = (
    "boot conf wait 5".split()
    + "start --run-number 101 wait 1 enable-triggers wait 100".split()
    + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop ".split()
    + "start --run-number 102 wait 1 enable-triggers wait 100".split()
    + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop ".split()
    + " scrap terminate".split()
)

# The tests themselves


def test_dunerc_success(run_dunerc, caplog):
    # checks for run control success, problems during pytest setup, etc.
    basic_checks.basic_checks(run_dunerc, caplog, print_test_name=False)


def test_log_files(run_dunerc):
    if check_for_logfile_errors:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(
            run_dunerc.log_files, True, True, ignored_logfile_problems,
            verbosity_helper=run_dunerc.verbosity_helper
        )


def test_data_files(run_dunerc):
    fragment_check_list = [triggercandidate_frag_params, hsi_frag_params, daphne_frag_params]
    fragment_check_list.append(daphne_triggerprimitive_frag_params)
    fragment_check_list.append(triggeractivity_frag_params)

    # Run some tests on the output data file
    all_ok = len(run_dunerc.data_files) == 6  # three for each run
    #print("") # Clear potential dot from pytest
    if all_ok:
        if run_dunerc.verbosity_helper.compare_level(IntegtestVerbosityLevels.drunc_transitions):
            print("\N{WHITE HEAVY CHECK MARK} The correct number of raw data files was found (6)")
    else:
        print(f"\N{POLICE CARS REVOLVING LIGHT} An incorrect number of raw data files was found, expected 6, found {len(run_dunerc.data_files)} \N{POLICE CARS REVOLVING LIGHT}")

    for idx in range(len(run_dunerc.data_files)):
        data_file = data_file_checks.DataFile(run_dunerc.data_files[idx], run_dunerc.verbosity_helper)
        all_ok &= data_file_checks.sanity_check(data_file)
        all_ok &= data_file_checks.check_file_attributes(data_file)
        for jdx in range(len(fragment_check_list)):
            all_ok &= data_file_checks.check_fragment_count(
                data_file, fragment_check_list[jdx]
            )
            all_ok &= data_file_checks.check_fragment_sizes(
                data_file, fragment_check_list[jdx]
            )
    assert all_ok, "\N{POLICE CARS REVOLVING LIGHT} One or more raw data file checks failed! \N{POLICE CARS REVOLVING LIGHT}"


def test_tpstream_files(run_dunerc):
    tpstream_files = run_dunerc.tpset_files
    fragment_check_list = [daphne_tpset_params]

    all_ok = len(tpstream_files) == 6  # three for each run
    #print("") # Clear potential dot from pytest
    if all_ok:
        if run_dunerc.verbosity_helper.compare_level(IntegtestVerbosityLevels.drunc_transitions):
            print("\N{WHITE HEAVY CHECK MARK} The correct number of TP-stream data files was found (6)")
    else:
        print(f"\N{POLICE CARS REVOLVING LIGHT} An incorrect number of TP-stream data files was found, expected 6, found {len(tpstream_files)} \N{POLICE CARS REVOLVING LIGHT}")

    for idx in range(len(tpstream_files)):
        data_file = data_file_checks.DataFile(tpstream_files[idx], run_dunerc.verbosity_helper)
        all_ok &= data_file_checks.check_file_attributes(data_file)
        for jdx in range(len(fragment_check_list)):
            all_ok &= data_file_checks.check_fragment_count(
                data_file, fragment_check_list[jdx]
            )
    assert all_ok, "\N{POLICE CARS REVOLVING LIGHT} One or more TP-stream data file checks failed! \N{POLICE CARS REVOLVING LIGHT}"


def test_cleanup(run_dunerc):
    pathlist_string = ""
    filelist_string = ""
    for data_file in run_dunerc.data_files:
        filelist_string += " " + str(data_file)
        if str(data_file.parent) not in pathlist_string:
            pathlist_string += " " + str(data_file.parent)
    for data_file in run_dunerc.tpset_files:
        filelist_string += " " + str(data_file)
        if str(data_file.parent) not in pathlist_string:
            pathlist_string += " " + str(data_file.parent)

    if pathlist_string and filelist_string:
        if run_dunerc.verbosity_helper.compare_level(IntegtestVerbosityLevels.integtest_debug):
            print("============================================")
            print("Listing the hdf5 files before deleting them:")
            print("============================================")

            os.system(f"df -h {pathlist_string}")
            print("--------------------")
            os.system(f"ls -alF {filelist_string}")

        for data_file in run_dunerc.data_files:
            data_file.unlink()
        for data_file in run_dunerc.tpset_files:
            data_file.unlink()

        if run_dunerc.verbosity_helper.compare_level(IntegtestVerbosityLevels.integtest_debug):
            print("--------------------")
            os.system(f"df -h {pathlist_string}")
            print("============================================")
