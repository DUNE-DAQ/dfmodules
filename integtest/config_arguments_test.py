import pytest

import integrationtest.log_file_checks as log_file_checks

base_arg_conf=['-o', '.']


# The next three variables declarations *must* be present as globals in the test
# file. They're read by the "fixtures" in conftest.py to determine how
# to run the config generation and nanorc

# The name of the python module for the config generation
confgen_name="minidaqapp.newconf.mdapp_multiru_gen"
# The arguments to pass to the config generator, excluding the json
# output directory (the test framework handles that)
# nanorc_command_list=["terminate"]
nanorc_command_list=["terminate"]

confgen_arguments={
    'partition-name'                        : ['-p', "${USER}_test2"],
    'global-partition-name'                 : ['-g', 'global2'],
    'host-global'                           : ['--host-global', 'np04-srv-013.cern.ch'],
    'port-global'                           : ['--port-global', '12346'],
    'n'                                     : ['-n', '3'],
    'emulator'                              : ['-e'],
    'slow-down-factor'                      : ['-s', '10'],
    'run-number'                            : ['-r', '334'],
    't'                                     : ['-t', '10.0'],
    'before'                                : ['-b', '10000'],
    'after'                                 : ['-a', '10000'],
    'c'                                     : ['-c', '100'],
    'frame.bin'                             : ['-d', './frames_2.bin'],
    'disable-trace'                         : ['--disable-trace'],
    'use-felix'                             : ['--use-felix'],
    'use-ssp'                               : ['--use-ssp'],
    'host-df'                               : ['--host-df', 'np04-srv-001'],
    'host-ru'                               : ['--host-ru', 'np04-srv-001'],
    'host-trigger'                          : ['--host-trigger', 'np04-srv-001'],
    'host-hsi'                              : ['--host-hsi', 'np04-srv-001'],
    'hist-tprtc'                            : ['--host-tprtc', 'np04-srv-001'],
    'region-id'                             : ['--region-id', '1'],
    'latency-buffer-size'                   : ['--latency-buffer-size', '444968'],
    'hsi-hw-connection-file'                : ['--hsi-hw-connections-file', "${TIMING_SHARE}/configggg/etc/connections.xml"],
    'hsi-device-name'                       : ['--hsi-device-name', "BOREAS"],
    'hsi-readout-period'                    : ['--hsi-readout-period', '2e3'],
    'control-hsi-hw'                        : ['--control-hsi-hw'],
    'hsi-endpoint-address'                  : ['--hsi-endpoint-address', '2'],
    'hsi-endpoint-partition'                : ['--hsi-endpoint-partition', '2'],
    'hsi-re-mask'                           : ['--hsi-re-mask', '0x1'],
    'hsi-fe-mask'                           : ['--hsi-fe-mask', '0x1'],
    'hsi-inv-mask'                          : ['--hsi-inv-mask', '0x1'],
    'hsi-source'                            : ['--hsi-source', '0x0'],
    'use-hsi-hw'                            : ['--use-hsi-hw'],
    'hsi-device-id'                         : ['--hsi-device-id', '1'],
    'mean-hsi-signal-multiplicity'          : ['--mean-hsi-signal-multiplicity', '2'],
    'hsi-signal-emulation-mode'             : ['--hsi-signal-emulation-mode', '1'],
    'enabled-hsi-signals'                   : ['--enabled-hsi-signals', '0b000000010'],
    'ttcm-s1'                               : ['--ttcm-s1', '2'],
    'ttcm-s2'                               : ['--ttcm-s2', '0'],
    'trigger-activity-plugin'               : ['--trigger-activity-plugin', 'TriggerActivityMakerPrescalePluginnn'],
    'trigger-activity-config'               : ['--trigger-activity-config', 'dict(prescale=1000],'],
    'trigger-activity-config'               : ['--trigger-candidate-plugin', 'TriggerCandidateMakerPrescalePluginn'],
    'trigger-candidate-config'              : ['--trigger-candidate-config', 'dict(prescale=1000],'],
    'control-timing-partition'              : ['--control-timing-partition'],
    'timing-partition-master-device-name'   : ['--timing-partition-master-device-name', "tpmdn"],
    'timing-partition-id'                   : ['--timing-partition-id', '1'],
    'timing-partition-trigger-mask'         : ['--timing-partition-trigger-mask', '0xdd'],
    'timing-partition-rate-control-enabled' : ['--timing-partition-rate-control-enabled', '1'],
    'timing-partition-spill-gate-enabled'   : ['--timing-partition-spill-gate-enabled', '1'],
    'enable-raw-recording'                  : ['--enable-raw-recording'],
    'raw-recording-output-dir'              : ['--raw-recording-output-dir', '/dev/null'],
    'frontend-type-wibs'                    : ['--frontend-type', 'wib2'],
    'frontend-type-pds-queue'               : ['--frontend-type', 'pds_queue'],
    'frontend-type-pds-list'                : ['--frontend-type', 'pds_list'],
    'frontend-type-pacman'                  : ['--frontend-type', 'pacman'],
    'frontend-type-ssp'                     : ['--frontend-type', 'ssp'],
    'enable-dqm'                            : ['--enable-dqm'],
    'opmon-impl-json'                       : ['--opmon-impl', 'json'],
    'opmon-impl-cern'                       : ['--opmon-impl', 'cern'],
    'opmon-impl-pocket'                     : ['--opmon-impl', 'pocket'],
    'ers-impl-cern'                         : ['--ers-impl', 'cern'],
    'ers-impl-local'                        : ['--ers-impl', 'local'],
    'ers-impl-pocket'                       : ['--ers-impl', 'pocket'],
    'dqm-impl-cern'                         : ['--dqm-impl', 'cern'],
    'dqm-impl-local'                        : ['--dqm-impl', 'local'],
    'dqm-impl-pocket'                       : ['--dqm-impl', 'pocket'],
    'pocket-url'                            : ['--pocket-url', '124.0.0.1'],
    'enable-software-tpg'                   : ['--enable-software-tpg'],
    'enable-tpset-writing'                  : ['--enable-tpset-writing'],
    'use-fake-data-producers'               : ['--use-fake-data-producers'],
    'dqm-cmap-HD'                           : ['--dqm-cmap', 'HD'],
    'dqm-cmap-VD'                           : ['--dqm-cmap', 'VD'],
    'dqm-rawdisplay-params'                 : ['--dqm-rawdisplay-params', '600', '100', '500'],
    'dqm-meanrms-params'                    : ['--dqm-meanrms-params', '100', '10', '1000'],
    'dqm-fourier-params'                    : ['--dqm-fourier-params', '6000', '600', '1000'],
    'dqm-fouriersum-params'                 : ['--dqm-fouriersum-params', '6000', '600', '10000'],
    'op-env'                                : ['--op-env', 'swtest2'],
    'tpc-region-name-prefix'                : ['--tpc-region-name-prefix', 'CRP'],
    'max-file-size'                         : ['--max-file-size', '1024']
}

confgen_arguments = {k:v+base_arg_conf for (k,v) in confgen_arguments.items()}

# Why do I need run_nanorc here?? I get
# In test_conf_params: function uses no fixture 'run_nanorc'
# if I don't include it
def test_conf_arg_effect(diff_conf_files, run_nanorc):
    assert diff_conf_files.diff != []

# Ditto for run_nanorc
def test_json_log(create_json_files, run_nanorc):
    logs = create_json_files.log_file
    assert log_file_checks.logs_are_error_free([logs])
      
