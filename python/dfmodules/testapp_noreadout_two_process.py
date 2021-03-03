# testapp_noreadout_two_process.py

# This python configuration produces *two* json configuration files
# that together form a MiniDAQApp with the same functionality as
# MiniDAQApp v1, but in two processes. One process contains the
# TriggerDecisionEmulator, while the other process contains everything
# else. The network communication is done with the QueueToNetwork and
# NetworkToQueue modules from the nwqueueadapters package.
#
# As with testapp_noreadout_confgen.py
# in this directory, no modules from the readout package are used: the
# fragments are provided by the FakeDataProd module from dfmodules


# Set moo schema search path
from dunedaq.env import get_moo_model_path
import moo.io
moo.io.default_load_path = get_moo_model_path()

# Load configuration types
import moo.otypes
moo.otypes.load_types('rcif/cmd.jsonnet')
moo.otypes.load_types('appfwk/cmd.jsonnet')
moo.otypes.load_types('appfwk/app.jsonnet')

moo.otypes.load_types('trigemu/triggerdecisionemulator.jsonnet')
moo.otypes.load_types('trigemu/faketimesyncsource.jsonnet')
moo.otypes.load_types('dfmodules/requestgenerator.jsonnet')
moo.otypes.load_types('dfmodules/fragmentreceiver.jsonnet')
moo.otypes.load_types('dfmodules/datawriter.jsonnet')
moo.otypes.load_types('dfmodules/hdf5datastore.jsonnet')
moo.otypes.load_types('dfmodules/fakedataprod.jsonnet')
moo.otypes.load_types('nwqueueadapters/queuetonetwork.jsonnet')
moo.otypes.load_types('nwqueueadapters/networktoqueue.jsonnet')
moo.otypes.load_types('serialization/networkobjectreceiver.jsonnet')
moo.otypes.load_types('serialization/networkobjectsender.jsonnet')

# Import new types
import dunedaq.cmdlib.cmd as basecmd # AddressedCmd, 
import dunedaq.rcif.cmd as rccmd # AddressedCmd, 
import dunedaq.appfwk.cmd as cmd # AddressedCmd, 
import dunedaq.appfwk.app as app # AddressedCmd,
import dunedaq.trigemu.triggerdecisionemulator as tde
import dunedaq.trigemu.faketimesyncsource as ftss
import dunedaq.dfmodules.requestgenerator as rqg
import dunedaq.dfmodules.fragmentreceiver as ffr
import dunedaq.dfmodules.datawriter as dw
import dunedaq.dfmodules.hdf5datastore as hdf5ds
import dunedaq.dfmodules.fakedataprod as fdp
import dunedaq.nwqueueadapters.networktoqueue as ntoq
import dunedaq.nwqueueadapters.queuetonetwork as qton
import dunedaq.serialization.networkobjectreceiver as nor
import dunedaq.serialization.networkobjectsender as nos

from appfwk.utils import mcmd, mrccmd, mspec

import json
import math
from pprint import pprint
# Time to waait on pop()
QUEUE_POP_WAIT_MS=100;
# local clock speed Hz
CLOCK_SPEED_HZ = 50000000;

def generate_df(
        NUMBER_OF_DATA_PRODUCERS=2,          
        DATA_RATE_SLOWDOWN_FACTOR = 1,
        RUN_NUMBER = 333, 
        TRIGGER_RATE_HZ = 1.0,
        DATA_FILE="./frames.bin",
        OUTPUT_PATH=".",
        DISABLE_OUTPUT=False,
        TOKEN_COUNT=10
    ):
    """Generate the json configuration for the readout and DF process"""
   
    trigger_interval_ticks = math.floor((1/TRIGGER_RATE_HZ) * CLOCK_SPEED_HZ/DATA_RATE_SLOWDOWN_FACTOR)

    # Define modules and queues
    queue_bare_specs = [
            app.QueueSpec(inst="time_sync_to_netq", kind='FollyMPMCQueue', capacity=100),
            app.QueueSpec(inst="token_to_netq", kind='FollySPSCQueue', capacity=20),
            app.QueueSpec(inst="trigger_decision_from_netq", kind='FollySPSCQueue', capacity=20),
            app.QueueSpec(inst="trigger_decision_copy_for_bookkeeping", kind='FollySPSCQueue', capacity=20),
            app.QueueSpec(inst="trigger_record_q", kind='FollySPSCQueue', capacity=20),
            app.QueueSpec(inst="data_fragments_q", kind='FollyMPMCQueue', capacity=100),
        ] + [
            app.QueueSpec(inst=f"data_requests_{idx}", kind='FollySPSCQueue', capacity=20)
                for idx in range(NUMBER_OF_DATA_PRODUCERS)
        ]
    

    # Only needed to reproduce the same order as when using jsonnet
    queue_specs = app.QueueSpecs(sorted(queue_bare_specs, key=lambda x: x.inst))


    mod_specs = [
        mspec("ntoq_trigdec", "NetworkToQueue", [
                        app.QueueInfo(name="output", inst="trigger_decision_from_netq", dir="output")
                    ]),

        mspec("qton_token", "QueueToNetwork", [
                        app.QueueInfo(name="input", inst="token_to_netq", dir="input")
                    ]),

        mspec("qton_timesync", "QueueToNetwork", [
                        app.QueueInfo(name="input", inst="time_sync_to_netq", dir="input")
                    ]),

        mspec("rqg", "RequestGenerator", [
                        app.QueueInfo(name="trigger_decision_input_queue", inst="trigger_decision_from_netq", dir="input"),
                        app.QueueInfo(name="trigger_decision_for_event_building", inst="trigger_decision_copy_for_bookkeeping", dir="output"),
                    ] + [
                        app.QueueInfo(name=f"data_request_{idx}_output_queue", inst=f"data_requests_{idx}", dir="output")
                            for idx in range(NUMBER_OF_DATA_PRODUCERS)
                    ]),

        mspec("ffr", "FragmentReceiver", [
                        app.QueueInfo(name="trigger_decision_input_queue", inst="trigger_decision_copy_for_bookkeeping", dir="input"),
                        app.QueueInfo(name="trigger_record_output_queue", inst="trigger_record_q", dir="output"),
                        app.QueueInfo(name="data_fragment_input_queue", inst="data_fragments_q", dir="input"),
                    ]),

        mspec("datawriter", "DataWriter", [
                        app.QueueInfo(name="trigger_record_input_queue", inst="trigger_record_q", dir="input"),
                        app.QueueInfo(name="token_output_queue", inst="token_to_netq", dir="output"),
                    ]),

        mspec("fake_timesync_source", "FakeTimeSyncSource", [
                        app.QueueInfo(name="time_sync_sink", inst="time_sync_to_netq", dir="output"),
                    ]),

        ] + [

                mspec(f"fakedataprod_{idx}", "FakeDataProd", [
                            app.QueueInfo(name="data_request_input_queue", inst=f"data_requests_{idx}", dir="input"),
                            app.QueueInfo(name="data_fragment_output_queue", inst="data_fragments_q", dir="output"),
                            ]) for idx in range(NUMBER_OF_DATA_PRODUCERS)
        ]

    init_specs = app.Init(queues=queue_specs, modules=mod_specs)

    initcmd = rccmd.RCCommand(
        id=basecmd.CmdId("init"),
        entry_state="NONE",
        exit_state="INITIAL",
        data=init_specs
    )

    confcmd = mrccmd("conf", "INITIAL", "CONFIGURED",[
                ("ntoq_trigdec", ntoq.Conf(msg_type="dunedaq::dfmessages::TriggerDecision",
                                           msg_module_name="TriggerDecisionNQ",
                                           receiver_config=nor.Conf(ipm_plugin_type="ZmqReceiver",
                                                                    address= "tcp://127.0.0.1:12345")
                                           )
                 ),

                ("qton_token", qton.Conf(msg_type="dunedaq::dfmessages::TriggerDecisionToken",
                                           msg_module_name="TriggerDecisionTokenNQ",
                                           sender_config=nos.Conf(ipm_plugin_type="ZmqSender",
                                                                  address= "tcp://127.0.0.1:12346",
                                                                  stype="msgpack")
                                           )
                 ),

                ("qton_timesync", qton.Conf(msg_type="dunedaq::dfmessages::TimeSync",
                                            msg_module_name="TimeSyncNQ",
                                            sender_config=nos.Conf(ipm_plugin_type="ZmqSender",
                                                                   address= "tcp://127.0.0.1:12347",
                                                                   stype="msgpack")
                                           )
                ),
        
                ("rqg", rqg.ConfParams(
                        map=rqg.mapgeoidqueue([
                                rqg.geoidinst(apa=0, link=idx, queueinstance=f"data_requests_{idx}") for idx in range(NUMBER_OF_DATA_PRODUCERS)
                            ])  
                        )),
                ("ffr", ffr.ConfParams(
                            general_queue_timeout=QUEUE_POP_WAIT_MS
                        )),
                ("datawriter", dw.ConfParams(
                            initial_token_count=TOKEN_COUNT,
                            data_store_parameters=hdf5ds.ConfParams(
                                name="data_store",
                                # type = "HDF5DataStore", # default
                                directory_path = OUTPUT_PATH, # default
                                # mode = "all-per-file", # default
                                max_file_size_bytes = 1073741834,
                                disable_unique_filename_suffix = False,
                                filename_parameters = hdf5ds.HDF5DataStoreFileNameParams(
                                    overall_prefix = "fake_minidaqapp",
                                    # digits_for_run_number = 6, #default
                                    file_index_prefix = "file"
                                ),
                                file_layout_parameters = hdf5ds.HDF5DataStoreFileLayoutParams(
                                    trigger_record_name_prefix= "TriggerRecord",
                                    digits_for_trigger_number = 5,
                                )
                            )
                        )),
                ("fake_timesync_source", ftss.ConfParams(
                    sync_interval_ticks = (CLOCK_SPEED_HZ/DATA_RATE_SLOWDOWN_FACTOR),
                    clock_frequency_hz = (CLOCK_SPEED_HZ/DATA_RATE_SLOWDOWN_FACTOR),
                  )),
            ] + [
                (f"fakedataprod_{idx}", fdp.ConfParams(
                        temporarily_hacked_link_number = idx
                        )) for idx in range(NUMBER_OF_DATA_PRODUCERS)
            ])

    startpars = rccmd.StartParams(run=RUN_NUMBER, disable_data_storage=DISABLE_OUTPUT)
    startcmd = mrccmd("start", "CONFIGURED", "RUNNING", [
            ("ntoq_trigdec", startpars),
            ("qton_token", startpars),
            ("qton_timesync", startpars),
            ("datawriter", startpars),
            ("ffr", startpars),
            ("fakedataprod_.*", startpars),
            ("rqg", startpars),
            ("fake_timesync_source", startpars),
        ])

    stopcmd = mrccmd("stop", "RUNNING", "CONFIGURED", [
            ("ntoq_trigdec", None),
            ("qton_timesync", None),
            ("qton_token", None),
            ("fake_timesync_source", None),
            ("rqg", None),
            ("fakedataprod_.*", None),
            ("ffr", None),
            ("datawriter", None),
        ])

    scrapcmd = mcmd("scrap", [
            ("", None)
        ])

    # Create a list of commands
    cmd_seq = [initcmd, confcmd, startcmd, stopcmd, scrapcmd]

    # Print them as json (to be improved/moved out)
    jstr = json.dumps([c.pod() for c in cmd_seq], indent=4, sort_keys=True)
    return jstr

#===============================================================================
def generate_trigemu(
        NUMBER_OF_DATA_PRODUCERS=2,          
        DATA_RATE_SLOWDOWN_FACTOR = 1,
        RUN_NUMBER = 333, 
        TRIGGER_RATE_HZ = 1.0,
        DATA_FILE="./frames.bin",
        OUTPUT_PATH=".",
    ):
    """Generate the json config for the TriggerDecisionEmulator process"""
    
    trigger_interval_ticks = math.floor((1/TRIGGER_RATE_HZ) * CLOCK_SPEED_HZ/DATA_RATE_SLOWDOWN_FACTOR)

    # Define modules and queues
    queue_bare_specs = [
            app.QueueSpec(inst="time_sync_from_netq", kind='FollySPSCQueue', capacity=100),
            app.QueueSpec(inst="token_from_netq", kind='FollySPSCQueue', capacity=20),
            app.QueueSpec(inst="trigger_decision_to_netq", kind='FollySPSCQueue', capacity=20),
        ]

    # Only needed to reproduce the same order as when using jsonnet
    queue_specs = app.QueueSpecs(sorted(queue_bare_specs, key=lambda x: x.inst))


    mod_specs = [
        mspec("qton_trigdec", "QueueToNetwork", [
                        app.QueueInfo(name="input", inst="trigger_decision_to_netq", dir="input")
                    ]),

        mspec("ntoq_token", "NetworkToQueue", [
                        app.QueueInfo(name="output", inst="token_from_netq", dir="output")
                    ]),

        mspec("ntoq_timesync", "NetworkToQueue", [
                        app.QueueInfo(name="output", inst="time_sync_from_netq", dir="output")
                    ]),

        mspec("tde", "TriggerDecisionEmulator", [
                        app.QueueInfo(name="time_sync_source", inst="time_sync_from_netq", dir="input"),
                        app.QueueInfo(name="token_source", inst="token_from_netq", dir="input"),
                        app.QueueInfo(name="trigger_decision_sink", inst="trigger_decision_to_netq", dir="output"),
                    ]),
        ]

    init_specs = app.Init(queues=queue_specs, modules=mod_specs)

    initcmd = rccmd.RCCommand(
        id=basecmd.CmdId("init"),
        entry_state="NONE",
        exit_state="INITIAL",
        data=init_specs
    )

    confcmd = mrccmd("conf", "INITIAL", "CONFIGURED",[
                ("qton_trigdec", qton.Conf(msg_type="dunedaq::dfmessages::TriggerDecision",
                                           msg_module_name="TriggerDecisionNQ",
                                           sender_config=nos.Conf(ipm_plugin_type="ZmqSender",
                                                                  address= "tcp://127.0.0.1:12345",
                                                                  stype="msgpack")
                                           )
                 ),

                 ("ntoq_token", ntoq.Conf(msg_type="dunedaq::dfmessages::TriggerDecisionToken",
                                            msg_module_name="TriggerDecisionTokenNQ",
                                            receiver_config=nor.Conf(ipm_plugin_type="ZmqReceiver",
                                                                     address= "tcp://127.0.0.1:12346")
                                            )
                 ),

                ("ntoq_timesync", ntoq.Conf(msg_type="dunedaq::dfmessages::TimeSync",
                                           msg_module_name="TimeSyncNQ",
                                           receiver_config=nor.Conf(ipm_plugin_type="ZmqReceiver",
                                                                    address= "tcp://127.0.0.1:12347")
                                           )
                ),

                ("tde", tde.ConfParams(
                        links=[idx for idx in range(NUMBER_OF_DATA_PRODUCERS)],
                        min_links_in_request=NUMBER_OF_DATA_PRODUCERS,
                        max_links_in_request=NUMBER_OF_DATA_PRODUCERS,
                        min_readout_window_ticks=1200,
                        max_readout_window_ticks=1200,
                        trigger_window_offset=1000,
                        # The delay is set to put the trigger well within the latency buff
                        trigger_delay_ticks=math.floor( 2* CLOCK_SPEED_HZ/DATA_RATE_SLOWDOWN_FACTOR),
                        # We divide the trigger interval by
                        # DATA_RATE_SLOWDOWN_FACTOR so the triggers are still
                        # emitted per (wall-clock) second, rather than being
                        # spaced out further
                        trigger_interval_ticks=trigger_interval_ticks,
                        clock_frequency_hz=CLOCK_SPEED_HZ/DATA_RATE_SLOWDOWN_FACTOR                    
                        )),
            ])

    startpars = rccmd.StartParams(run=RUN_NUMBER, disable_data_storage=False)
    startcmd = mrccmd("start", "CONFIGURED", "RUNNING", [
            ("qton_trigdec", startpars),
            ("ntoq_token", startpars),
            ("ntoq_timesync", startpars),
            ("tde", startpars),
        ])

    stopcmd = mrccmd("stop", "RUNNING", "CONFIGURED", [
            ("qton_trigdec", None),
            ("ntoq_timesync", None),
            ("ntoq_token", None),
            ("tde", None),
        ])

    pausecmd = mrccmd("pause", "RUNNING", "RUNNING", [
            ("", None)
        ])

    resumecmd = mrccmd("resume", "RUNNING", "RUNNING", [
            ("tde", tde.ResumeParams(
                            trigger_interval_ticks=trigger_interval_ticks
                        ))
        ])

    scrapcmd = mcmd("scrap", [
            ("", None)
        ])

    # Create a list of commands
    cmd_seq = [initcmd, confcmd, startcmd, stopcmd, pausecmd, resumecmd, scrapcmd]

    # Print them as json (to be improved/moved out)
    jstr = json.dumps([c.pod() for c in cmd_seq], indent=4, sort_keys=True)
    return jstr

if __name__ == '__main__':
    # Add -h as default help option
    CONTEXT_SETTINGS = dict(help_option_names=['-h', '--help'])

    import click

    @click.command(context_settings=CONTEXT_SETTINGS)
    @click.option('-n', '--number-of-data-producers', default=2)
    @click.option('-s', '--data-rate-slowdown-factor', default=1)
    @click.option('-r', '--run-number', default=333)
    @click.option('-t', '--trigger-rate-hz', default=1.0)
    @click.option('-d', '--data-file', type=click.Path(), default='./frames.bin')
    @click.option('-o', '--output-path', type=click.Path(), default='.')
    @click.option('--disable-data-storage', is_flag=True)
    @click.option('-c', '--token-count', default=10)
    @click.argument('json_file_base', type=click.Path(), default='testapp-noreadout-two-process')
    def cli(number_of_data_producers, data_rate_slowdown_factor, run_number, trigger_rate_hz, data_file, output_path, disable_data_storage, token_count, json_file_base):
        """
          JSON_FILE: Input raw data file.
          JSON_FILE: Output json configuration file.
        """

        json_file_trigemu=json_file_base+"-trigemu.json"
        json_file_df=json_file_base+"-df.json"
                   
        with open(json_file_trigemu, 'w') as f:
            f.write(generate_trigemu(
                    NUMBER_OF_DATA_PRODUCERS = number_of_data_producers,
                    DATA_RATE_SLOWDOWN_FACTOR = data_rate_slowdown_factor,
                    RUN_NUMBER = run_number, 
                    TRIGGER_RATE_HZ = trigger_rate_hz,
                    DATA_FILE = data_file,
                    OUTPUT_PATH = output_path
                ))

        with open(json_file_df, 'w') as f:
            f.write(generate_df(
                    NUMBER_OF_DATA_PRODUCERS = number_of_data_producers,
                    DATA_RATE_SLOWDOWN_FACTOR = data_rate_slowdown_factor,
                    RUN_NUMBER = run_number, 
                    TRIGGER_RATE_HZ = trigger_rate_hz,
                    DATA_FILE = data_file,
                    OUTPUT_PATH = output_path,
                    DISABLE_OUTPUT = disable_data_storage,
                    TOKEN_COUNT = token_count
                ))

    cli()
    
