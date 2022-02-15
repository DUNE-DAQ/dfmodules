import pytest

import integrationtest.log_file_checks as log_file_checks

# Values that help determine the running conditions
number_of_data_producers=2
run_duration=20  # seconds

# Default values for validation parameters
expected_number_of_data_files=1
check_for_logfile_errors=True
expected_event_count=run_duration
expected_event_count_tolerance=2
wib1_frag_hsi_trig_params={"fragment_type_description": "WIB",
                           "hdf5_detector_group": "TPC", "hdf5_region_prefix": "APA",
                           "expected_fragment_count": number_of_data_producers,
                           "min_size_bytes": 37200, "max_size_bytes": 37200}

# The next three variable declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="minidaqapp.newconf.mdapp_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
base_conf_args=['-o', '.']
confgen_arguments=[# base_conf_args+['-g', 'global2'],
                   # base_conf_args+['--host-global', 'np04-srv-013.cern.ch'],
                   # base_conf_args+['--port-global', 12346],
                   ['-p', "${USER}_test2"]+base_conf_args,
                   ['-n', '3']+base_conf_args,
                   # base_conf_args+['-e'],
                   # base_conf_args+['-s', 10],
                   # base_conf_args+['-r', 334],
                   # base_conf_args+['-t', 10.0],
                   # base_conf_args+['-b', 10000],
                   # base_conf_args+['-a', 10000],
                   # base_conf_args+['-c', 100],
                   # base_conf_args+['-d', './frames_2.bin'],
                   # base_conf_args+['-o', '/dev/null'],
                   # base_conf_args+['--disable-trace'],
                   # base_conf_args+['--use-felix'],
                   # base_conf_args+['--use-ssp'],
                   # base_conf_args+['--host-df', 'np04-srv-001'],
                   # base_conf_args+['--host-ru', 'np04-srv-001'],
                   # base_conf_args+['--host-trigger', 'np04-srv-001'],
                   # base_conf_args+['--host-hsi', 'np04-srv-001'],
                   # base_conf_args+['--host-tprtc', 'np04-srv-001'],
                   # base_conf_args+['--region-id', 1],
                   # base_conf_args+['--latency-buffer-size', 444968],
                   # base_conf_args+['--hsi-hw-connections-file', "${TIMING_SHARE}/configggg/etc/connections.xml"],
                   # base_conf_args+['--hsi-device-name', "BOREAS"],
                   # base_conf_args+['--hsi-readout-period', 2e3],
                   # base_conf_args+['--control-hsi-hw'],
                   # base_conf_args+['--hsi-endpoint-address', 2],
                   # base_conf_args+['--hsi-endpoint-partition', 2],
                   # base_conf_args+['--hsi-re-mask', 0x1],
                   # base_conf_args+['--hsi-fe-mask', 0x1],
                   # base_conf_args+['--hsi-inv-mask', 0x1],
                   # base_conf_args+['--hsi-source', 0x0],
                   # base_conf_args+['--use-hsi-hw'],
                   # base_conf_args+['--hsi-device-id', 1],
                   # base_conf_args+['--mean-hsi-signal-multiplicity', 2],
                   # base_conf_args+['--hsi-signal-emulation-mode', 1],
                   # base_conf_args+['--enabled-hsi-signals', 0b000000010],
                   # base_conf_args+['--ttcm-s1', 2],
                   # base_conf_args+['--ttcm-s2', 0],
                   # base_conf_args+['--trigger-activity-plugin', 'TriggerActivityMakerPrescalePluginnn'],
                   # base_conf_args+['--trigger-activity-config', 'dict(prescale=1000],'],
                   # base_conf_args+['--trigger-candidate-plugin', 'TriggerCandidateMakerPrescalePluginn'],
                   # base_conf_args+['--trigger-candidate-config', 'dict(prescale=1000],'],
                   # base_conf_args+['--control-timing-partition'],
                   # base_conf_args+['--timing-partition-master-device-name', "tpmdn"],
                   # base_conf_args+['--timing-partition-id', 1],
                   # base_conf_args+['--timing-partition-trigger-mask', 0xdd],
                   # base_conf_args+['--timing-partition-rate-control-enabled', 1],
                   # base_conf_args+['--timing-partition-spill-gate-enabled', 1],
                   # base_conf_args+['--enable-raw-recording'],
                   # base_conf_args+['--raw-recording-output-dir', '/dev/null'],
    ['--frontend-type', 'wib']+base_conf_args,
                   # base_conf_args+['--frontend-type', 'wib2'],
                   # base_conf_args+['--frontend-type', 'pds_queue'],
                   # base_conf_args+['--frontend-type', 'pds_list'],
                   # base_conf_args+['--frontend-type', 'pacman'],
                   # base_conf_args+['--frontend-type', 'ssp'],
                   # base_conf_args+['--enable-dqm'],
                   # base_conf_args+['--opmon-impl', 'json'],
                   # base_conf_args+['--opmon-impl', 'cern'],
                   # base_conf_args+['--opmon-impl', 'pocket'],
                   # base_conf_args+['--ers-impl', 'cern'],
                   # base_conf_args+['--ers-impl', 'local'],
                   # base_conf_args+['--ers-impl', 'pocket'],
                   # base_conf_args+['--dqm-impl', 'cern'],
                   # base_conf_args+['--dqm-impl', 'local'],
                   # base_conf_args+['--dqm-impl', 'pocket'],
                   # base_conf_args+['--pocket-url', '124.0.0.1'],
                   # base_conf_args+['--enable-software-tpg'],
                   # base_conf_args+['--enable-tpset-writing'],
                   # base_conf_args+['--use-fake-data-producers'],
                   # base_conf_args+['--dqm-cmap', 'HD'],
                   # base_conf_args+['--dqm-cmap', 'VD'],
                   # base_conf_args+['--dqm-rawdisplay-params', 600, 100, 500],
                   # base_conf_args+['--dqm-meanrms-params', 100, 10, 1000],
                   # base_conf_args+['--dqm-fourier-params', 6000, 600, 1000],
                   # base_conf_args+['--dqm-fouriersum-params', 6000, 600, 10000],
                   # base_conf_args+['--op-env', 'swtest2'],
                   # base_conf_args+['--tpc-region-name-prefix', 'CRP'],
                   # base_conf_args+['--max-file-size', '1024']
]

# The tests themselves
def test_conf_params(diff_conf_files):
    # Check that nanorc completed correctly
    print(diff_conf_files.diff)
    assert diff_conf_files.diff != []

def test_warning_error_in_json_log(create_json_files_log):
    logs = create_json_files_log.log_file
    assert log_file_checks.logs_are_error_free([logs])
      
