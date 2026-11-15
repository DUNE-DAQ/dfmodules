"""
Integration Test for DFO Protocol with a killed DF application

This test module validates that a DAQ system with multiple DFO applications keeps taking data
when one of the DF applications is killed, and that the run can then be stopped.

Run control does not send commands to a session in which an application cannot be reached, so
the killed DF application is excluded from the session before triggers are disabled.
"""

import copy
import os
import pytest

import integrationtest.data_classes as data_classes
import integrationtest.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import integrationtest.resource_validation as resource_validation
import integrationtest.utility_functions as utility_functions
from integrationtest.get_pytest_tmpdir import get_pytest_tmpdir

import functools

print = functools.partial(print, flush=True)  # always flush print() output

pytest_plugins = "integrationtest.integrationtest_drunc"

# Run setup
run_duration = 60  # seconds
check_for_logfile_errors = True

# Default values for validation parameters
number_of_dataflow_apps = 3
number_of_data_producers = 4
number_of_readout_apps = 2
trigger_rate = 4.0
# the file of the killed DF app is never closed
expected_number_of_data_files = number_of_dataflow_apps - 1
# A DF app that survives gets a third of the triggers before the kill and half of them after it. The kill
# lands between a quarter and three fifths of the way through the time with triggers enabled.
surviving_dataflow_app_share = 0.425
expected_event_count = run_duration * trigger_rate * surviving_dataflow_app_share
expected_event_count_tolerance = expected_event_count / 8
ta_prescale = 1000

wibeth_frag_params = {
    "fragment_type_description": "WIBEth",
    "fragment_type": "WIBEth",
    "expected_fragment_count": number_of_readout_apps * number_of_data_producers,
    "min_size_bytes": 7272,
    "max_size_bytes": 28872,
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
    "debug_mask": 0x0,
    "frag_sizes_by_TC_type": {"kPrescale": {"min_size_bytes": 208, "max_size_bytes": 264},
                                "kRandom": {"min_size_bytes": 128, "max_size_bytes": 264},
                                "default": {"min_size_bytes": 128, "max_size_bytes": 264} }
}
# sizes:  72 is for an empty TP fragment
#        168 is for a fragment with four TPs in it (72+24+24+24+24)
triggerprimitive_frag_params = {
    "fragment_type_description": "Trigger Primitive",
    "fragment_type": "Trigger_Primitive",
    "expected_fragment_count": 3 * number_of_readout_apps,
    "min_size_bytes": 72,
    "max_size_bytes": 168,
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
                              "default": {"min_size_bytes":  72, "max_size_bytes": 100} },
    # The fake HSI data is sparse, so the readout window always extends past the newest HSI data.
    # A kTiming trigger gets the one HSI frame that caused it (kIncomplete, kRequestWindowAfterBuffer);
    # other triggers get an empty fragment (kRequestWindowAfterBuffer, kEmptyFragment), or that same
    # frame (kIncomplete) when they come within a readout window of a kTiming trigger.
    "frag_bitmasks_by_TC_type": {"kTiming": {"status_bitmask": 0xFFFFFFDD},
                                 "default": {"status_bitmask": 0xFFFFFF9D} }
}
# The application to kill, and where it is in the run control tree
killed_application = "df-02"
killed_application_path = "root-controller/df-controller/" + killed_application
# An exclude sent through a controller also stops the commands to that controller (drunc marks the
# controller itself as excluded). Including an application that is still alive in the same segment
# brings the controller back without bringing back the killed application.
surviving_application_path = "root-controller/df-controller/df-01"

ignored_logfile_problems = {
    # run control reports the killed application until it is excluded, and when it is excluded
    "-controller": [
        rf"'{killed_application}' does not ping",
        rf"Could not resolve REST URI for '{killed_application}'",
        rf"Disconnected child node\(s\) while handling '\w+': {killed_application}",
    ],
    "drunc-unified-shell|process_manager": [
        rf"Process '{killed_application}' .* was terminated unexpectedly",
        rf"Process {killed_application} of with UUID \S+ has died",
    ],
    # the DFOs report the killed DF app as silent, and fail to send it status requests until then
    "_dfo-0": [
        rf"No DataflowStatus received from trigger_decision_{killed_application} for \d+ ms",
        r"DFOModule::send_status_requests\(.*DFO-dfo-\d+$",
        r"NetworkSenderModel<Datatype>::send\(.*Datatype = dunedaq::dfmessages::DataflowStatusRequest",
        rf"dataflow_status_request_{killed_application}: Unable to send within timeout period",
        rf"dataflow_status_request_{killed_application} already present in the node list",
    ],
    "local-connection-server": [
        "errorlog: -",
    ],
    # 04-Mar-2026, KAB: added the absl::InitializeLog warning message to the ignored list for
    # all DAQ processes, given that we currently don't have a way suppress it at its source.
    r".*": [
        r"WARNING: All log messages before absl::InitializeLog\(\) is called are written to STDERR"
    ]
}
# Determine if this computer has enough resources for these tests
resource_validator = resource_validation.ResourceValidator()
resource_validator.cpu_count_needs(
    15, 30
)  # 3 for each data source (incl TPG) plus 3 more for everything else
resource_validator.free_memory_needs(
    9, 14
)  # 30% more than what we observe being used ('free -h')
actual_output_path = get_pytest_tmpdir()
resource_validator.free_disk_space_needs(
    actual_output_path, 1
)  # more than what we observe

### Config setup
common_config_obj = data_classes.integtest_params_for_predefined_dunedaq_config()
common_config_obj.op_env = "test"
common_config_obj.predefined_config_db ="config/daqsystemtest/example-configs.data.xml"

common_config_obj.config_substitutions.append(
    data_classes.attribute_substitution(
        obj_class="TCDataProcessor",     # 12-Nov-2025, KAB: turned off the merging of
        obj_id="def-tc-processor",       # overlapping TCs so that we get more consistent
        updates={                        # numbers of TriggerRecords in the output files.
            "merge_overlapping_tcs": False
        },)
)

# Get default config
multidfo_local_conf = copy.deepcopy(common_config_obj)
multidfo_local_conf.config_session_name = "local-multidfo-config"

# The delay of a signal is counted from the launch of the run control session, before boot. Boot, conf and
# start have taken between 25 s and 45 s, depending on the load of the host, so a signal at 60 s lands
# 15 s to 35 s after triggers are enabled, with 25 s to 45 s of run_duration still to go.
signal_delay = 60  # seconds

killed_df_conf = copy.deepcopy(multidfo_local_conf)
killed_df_conf.system_signal_configs = [
    data_classes.system_signal_config(
        application_label=killed_application, signal=data_classes.PosixSignal.SIGKILL, delay_s=signal_delay
    ),
]

confgen_arguments = {
    "killed-df": killed_df_conf,
}

# The commands to run in dunerc, as a list
dunerc_command_list = (
    "boot wait 2 conf start --run-number 101 wait 1 enable-triggers wait ".split()
    + [str(run_duration)]
    + ["exclude", "--target", killed_application_path]
    + ["include", "--target", surviving_application_path]
    + "disable-triggers wait 2 drain-dataflow wait 2 stop-trigger-sources stop scrap terminate".split()
)


### Tests

def test_dunerc_success(run_dunerc, caplog):
    # checks for run control success, problems during pytest setup, etc.
    utility_functions.basic_checks(run_dunerc, caplog, print_test_name=True)


def test_log_files(run_dunerc):
    if check_for_logfile_errors:
        # Check that there are no warnings or errors in the log files
        assert log_file_checks.logs_are_error_free(
            run_dunerc.log_files, True, True, ignored_logfile_problems,
            verbosity_helper=run_dunerc.verbosity_helper
        )


def test_data_files(run_dunerc):
    local_expected_event_count = expected_event_count
    local_event_count_tolerance = expected_event_count_tolerance
    low_number_of_files = expected_number_of_data_files
    high_number_of_files = expected_number_of_data_files
    fragment_check_list = [triggercandidate_frag_params, hsi_frag_params, wibeth_frag_params]

    local_expected_event_count += (
        (6250.0 / ta_prescale)
        * number_of_data_producers
        * number_of_readout_apps
        * run_duration
        * surviving_dataflow_app_share
        / 100
    )
    local_event_count_tolerance += (
        (250.0 / ta_prescale)
        * number_of_data_producers
        * number_of_readout_apps
        * run_duration
        * surviving_dataflow_app_share
        / 100
    )
    fragment_check_list.append(triggerprimitive_frag_params)
    nontrig_fragment_check_list = [hsi_frag_params, wibeth_frag_params]

    # Run some tests on the output data file
    assert (
        len(run_dunerc.data_files) == high_number_of_files
        or len(run_dunerc.data_files) == low_number_of_files
    )

    all_ok = True
    files_by_record = {}
    for idx in range(len(run_dunerc.data_files)):
        data_file = data_file_checks.DataFile(run_dunerc.data_files[idx], run_dunerc.verbosity_helper)
        # the record names hold the trigger number and the sequence number, e.g. TriggerRecord00012.0000
        for record_name in data_file.events:
            files_by_record.setdefault(record_name, []).append(os.path.basename(data_file.name))
        all_ok &= data_file_checks.sanity_check(data_file)
        all_ok &= data_file_checks.check_file_attributes(data_file)
        all_ok &= data_file_checks.check_event_count(
            data_file, local_expected_event_count, local_event_count_tolerance
        )
        for jdx in range(len(fragment_check_list)):
            all_ok &= data_file_checks.check_fragment_count(
                data_file, fragment_check_list[jdx]
            )
            all_ok &= data_file_checks.check_fragment_sizes(
                data_file, fragment_check_list[jdx]
            )
        for kdx in range(len(nontrig_fragment_check_list)):
            all_ok &= data_file_checks.check_fragment_error_flags( data_file, nontrig_fragment_check_list[kdx])

    # Each DFO sends its own TriggerDecision for a trigger, but only one DF app may build and write it
    for record_name, file_names in sorted(files_by_record.items()):
        if len(file_names) > 1:
            print(f"\N{POLICE CARS REVOLVING LIGHT} {record_name} is in more than one file: {file_names} \N{POLICE CARS REVOLVING LIGHT}")
            all_ok = False
    assert all_ok
