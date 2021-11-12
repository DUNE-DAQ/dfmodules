#!/usr/bin/env python3

import dfmodules.data_file_checks as data_file_checks
import integrationtest.log_file_checks as log_file_checks
import os
import json
import math
import re
import rich.traceback
from rich.console import Console
from os.path import exists, join

# Add -h as default help option
CONTEXT_SETTINGS = dict(help_option_names=['-h', '--help'])

console = Console()

import click

@click.command(context_settings=CONTEXT_SETTINGS)
@click.option('-t', '--check-tps', default=True, help="Check for TPSet Fragments in output")
@click.option('-n', '--number-of-data-producers', default=2, help="Number of links in output")
@click.option('-l', '--check-for-logfile-errors', default=True, help="Whether to check for errors in the log files")
@click.option('-d', '--run_duration', default=60, help="How long the runs lasted")
@click.option('--trigger-rate', default=1, help="The trigger rate, in Hz")
@click.argument('run_dir', type=click.Path(exists=True), default=os.curdir)

def cli(check_tps, number_of_data_producers, check_for_logfile_errors, run_duration, trigger_rate, run_dir):

    dirfiles = [ join(run_dir, f) for f in os.listdir(run_dir) if os.path.isfile(f) ]
    log_files = [ f for f in dirfiles if "log_" in f]
    data_files = [ f for f in dirfiles if ".hdf5" in f]


    current_td_match = 0
    current_run_match = 0
    run_trigger_counts = {}

    for f in log_files:
        if "log_trigger" in f:
            for line in open(f).readlines():
                match_tds = re.search(r"Sent (\d+) TDs\.", line)
                if match_tds != None:
                    current_td_match = match_tds.group(1)
                match_run = re.search(r"End of run (\d+)", line)
                if match_run != None:
                    current_run_match = match_run.group(1)

                if current_td_match != 0 and current_run_match != 0:
                    run_trigger_counts[current_run_match] = current_td_match
                    current_run_match = 0
                    current_td_match = 0
                

    print(run_trigger_counts)
    expected_event_count = run_duration * trigger_rate
    expected_event_count_tolerance = 2

    wib1_frag_hsi_trig_params = {"fragment_type_description": "WIB", "hdf5_groups": "TPC/APA000",
                           "element_name_prefix": "Link", "element_number_offset": 0,
                           "expected_fragment_count": number_of_data_producers,
                           "min_size_bytes": 37200, "max_size_bytes": 37200}
    wib1_frag_multi_trig_params = {"fragment_type_description": "WIB", "hdf5_groups": "TPC/APA000",
                             "element_name_prefix": "Link", "element_number_offset": 0,
                             "expected_fragment_count": number_of_data_producers,
                             "min_size_bytes": 80, "max_size_bytes": 37200}
    rawtp_frag_params = {"fragment_type_description": "Raw TP", "hdf5_groups": "TPC/TP_APA000",
                   "element_name_prefix": "Link", "element_number_offset": 0,
                   "expected_fragment_count": number_of_data_producers,
                   "min_size_bytes": 80, "max_size_bytes": 80}
    triggertp_frag_params = {"fragment_type_description": "Trigger TP", "hdf5_groups": "Trigger/Region000",
                       "element_name_prefix": "Element", "element_number_offset": 0,
                       "expected_fragment_count": number_of_data_producers,
                       "min_size_bytes": 80, "max_size_bytes": 80}

    def test_log_files():
       if check_for_logfile_errors:
           # Check that there are no warnings or errors in the log files
           log_file_checks.logs_are_error_free(log_files)

    def test_data_file():
       local_expected_event_count = expected_event_count
       local_event_count_tolerance = expected_event_count_tolerance
       fragment_check_list = []
       if check_tps:
           local_expected_event_count+=(270 * number_of_data_producers * run_duration / 100)
           local_event_count_tolerance+=(10 * number_of_data_producers * run_duration / 100)
           fragment_check_list.append(wib1_frag_multi_trig_params)
           fragment_check_list.append(rawtp_frag_params)
           fragment_check_list.append(triggertp_frag_params)
       if len(fragment_check_list) == 0:
           fragment_check_list.append(wib1_frag_hsi_trig_params)

       for idx in range(len(data_files)):
           data_file = data_file_checks.DataFile(data_files[idx])
           data_file_checks.sanity_check(data_file)
           data_file_checks.check_event_count(data_file, local_expected_event_count, local_event_count_tolerance)
           for jdx in range(len(fragment_check_list)):
               data_file_checks.check_fragment_count(data_file, fragment_check_list[jdx])
               data_file_checks.check_fragment_presence(data_file, fragment_check_list[jdx])
               data_file_checks.check_fragment_size2(data_file, fragment_check_list[jdx])

    test_log_files()
    test_data_file()

if __name__ == '__main__':
    try:
        cli(show_default=True, standalone_mode=True)
    except Exception as e:
        console.print_exception()
