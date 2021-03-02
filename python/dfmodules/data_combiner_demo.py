# Set moo schema search path
from dunedaq.env import get_moo_model_path
import moo.io
moo.io.default_load_path = get_moo_model_path()

# Load configuration types
import moo.otypes

moo.otypes.load_types('rcif/cmd.jsonnet')
moo.otypes.load_types('appfwk/cmd.jsonnet')
moo.otypes.load_types('appfwk/app.jsonnet')

moo.otypes.load_types('dfmodules/hdf5datastore.jsonnet')
moo.otypes.load_types('dfmodules/datatransfermodule.jsonnet')

# Import new types
import dunedaq.cmdlib.cmd as basecmd # AddressedCmd, 
import dunedaq.rcif.cmd as rccmd # AddressedCmd, 
import dunedaq.appfwk.cmd as cmd # AddressedCmd, 
import dunedaq.appfwk.app as app # AddressedCmd, 
import dunedaq.dfmodules.hdf5datastore as hdf5ds
import dunedaq.dfmodules.datatransfermodule as dtm

from appfwk.utils import mcmd, mrccmd, mspec

import json
import math

def generate(
        RUN_NUMBER = 333, 
        DIRECTORY_PATH=".",
    ):

    # Define modules and queues
    queue_bare_specs = []

    # Only needed to reproduce the same order as when using jsonnet
    queue_specs = app.QueueSpecs(sorted(queue_bare_specs, key=lambda x: x.inst))

    mod_specs = [mspec("dataxfer", "DataTransferModule", []),]

    init_specs = app.Init(queues=queue_specs, modules=mod_specs)

    jstr = json.dumps(init_specs.pod(), indent=4, sort_keys=True)
    print(jstr)

    initcmd = rccmd.RCCommand(
        id=basecmd.CmdId("init"),
        entry_state="NONE",
        exit_state="INITIAL",
        data=init_specs
    )

    confcmd = mrccmd("conf", "INITIAL", "CONFIGURED",[
                ("dataxfer", dtm.ConfParams(
                            input_data_store_parameters=hdf5ds.ConfParams(
                                name="input_store",
                                type = "HDF5DataStore",
                                directory_path = DIRECTORY_PATH,
                                mode = "one-fragment-per-file",
                                filename_parameters = hdf5ds.HDF5DataStoreFileNameParams(
                                    overall_prefix = "generator_demo",
                                    file_index_prefix = "file"
                                ),
                                file_layout_parameters = hdf5ds.HDF5DataStoreFileLayoutParams(
                                    trigger_record_name_prefix= "TriggerRecord",
                                    digits_for_trigger_number = 5,
                                )
                            ),
                            output_data_store_parameters=hdf5ds.ConfParams(
                                name="output_store",
                                type = "HDF5DataStore",
                                directory_path = DIRECTORY_PATH,
                                mode = "one-event-per-file",
                                filename_parameters = hdf5ds.HDF5DataStoreFileNameParams(
                                    overall_prefix = "combiner_demo",
                                    file_index_prefix = "file"
                                ),
                                file_layout_parameters = hdf5ds.HDF5DataStoreFileLayoutParams(
                                    trigger_record_name_prefix= "TriggerRecord",
                                    digits_for_trigger_number = 5,
                                )
                            )
                        )),
            ])
    
    jstr = json.dumps(confcmd.pod(), indent=4, sort_keys=True)
    print(jstr)

    startpars = rccmd.StartParams(run=RUN_NUMBER)
    startcmd = mrccmd("start", "CONFIGURED", "RUNNING", [
            ("dataxfer", startpars),
        ])

    jstr = json.dumps(startcmd.pod(), indent=4, sort_keys=True)
    print("="*80+"\nStart\n\n", jstr)

    emptypars = rccmd.EmptyParams()

    stopcmd = mrccmd("stop", "RUNNING", "CONFIGURED", [
            ("dataxfer", emptypars),
        ])

    jstr = json.dumps(stopcmd.pod(), indent=4, sort_keys=True)
    print("="*80+"\nStop\n\n", jstr)

    pausecmd = mrccmd("pause", "RUNNING", "RUNNING", [
            ("", emptypars)
        ])

    jstr = json.dumps(pausecmd.pod(), indent=4, sort_keys=True)
    print("="*80+"\nPause\n\n", jstr)

    resumecmd = mrccmd("resume", "RUNNING", "RUNNING", [
            ("", emptypars)
        ])

    jstr = json.dumps(resumecmd.pod(), indent=4, sort_keys=True)
    print("="*80+"\nResume\n\n", jstr)

    scrapcmd = mcmd("scrap", [
            ("", emptypars)
        ])

    jstr = json.dumps(scrapcmd.pod(), indent=4, sort_keys=True)
    print("="*80+"\nScrap\n\n", jstr)

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
    @click.option('-r', '--run-number', default=333)
    @click.option('-d', '--directory-path', type=click.Path(), default='.')
    @click.argument('json_file', type=click.Path(), default='data_combiner_demo.json')
    def cli(run_number, directory_path, json_file):
        """
          JSON_FILE: Input raw data file.
          JSON_FILE: Output json configuration file.
        """

        with open(json_file, 'w') as f:
            f.write(generate(
                    RUN_NUMBER = run_number, 
                    DIRECTORY_PATH = directory_path,
                ))

        print(f"'{json_file}' generation completed.")

    cli()
