import json
import os
import rich.traceback
from rich.console import Console
from os.path import exists, join


# Add -h as default help option
CONTEXT_SETTINGS = dict(help_option_names=['-h', '--help'])

console = Console()

def generate_boot(producer_spec: dict,
                  writer_spec: dict) -> dict:

    daq_app_specs = {
        "daq_application_ups" : {
            "comment": "Application profile based on a full dbt runtime environment",
            "env": {
               "DBT_AREA_ROOT": "getenv",
               "TRACE_FILE": "getenv:/tmp/trace_buffer_${HOSTNAME}_${USER}"
            },
            "cmd": [
                "CMD_FAC=rest://localhost:${APP_PORT}",
                "INFO_SVC=file://info_${APP_ID}_${APP_PORT}.json",
                "cd ${DBT_AREA_ROOT}",
                "source dbt-setup-env.sh",
                "dbt-setup-runtime-environment",
                "cd ${APP_WD}",
                "daq_application --name ${APP_ID} -c ${CMD_FAC} -i ${INFO_SVC}"
            ]
        },
        "daq_application" : {
            "comment": "Application profile using  PATH variables (lower start time)",
            "env":{
                "CET_PLUGIN_PATH": "getenv",
                "DUNEDAQ_SHARE_PATH": "getenv",
                "LD_LIBRARY_PATH": "getenv",
                "PATH": "getenv",
                "DUNEDAQ_ERS_DEBUG_LEVEL": "15",
                "TRACE_FILE": "getenv:/tmp/trace_buffer_${HOSTNAME}_${USER}"
            },
            "cmd": [
                "CMD_FAC=rest://localhost:${APP_PORT}",
                "INFO_SVC=file://info_${APP_NAME}_${APP_PORT}.json",
                "cd ${APP_WD}",
                "daq_application --name ${APP_NAME} -c ${CMD_FAC} -i ${INFO_SVC}"
            ]
        }
    }

    boot = {
        "env": {
            "DUNEDAQ_ERS_VERBOSITY_LEVEL": 1
        },
        "apps": {
            producer_spec["name"]: {
                "exec": "daq_application",
                "host": "host_producer",
                "port": producer_spec["port"]
            },
            writer_spec["name"]: {
                "exec": "daq_application",
                "host": "host_writer",
                "port": writer_spec["port"]
            }
        },
        "hosts": {
            "host_producer": producer_spec["host"],
            "host_writer": writer_spec["host"]
        },
        "response_listener": {
            "port": 56789
        },
        "exec": daq_app_specs
    }

    console.log("Boot data")
    console.log(boot)
    return boot


import click

@click.command(context_settings=CONTEXT_SETTINGS)
@click.option('-s', '--slowdown-factor', default=1.0)
@click.option('-f', '--input-file', type=click.Path())
@click.option('--producer-host', default="localhost")
@click.option('--writer-host', default="localhost")
@click.argument('json_dir', type=click.Path())
def cli(slowdown_factor, input_file, producer_host, writer_host, json_dir):
    """
      JSON_DIR: Json file output folder
    """
    console.log("Loading tpset_producer config generator")
    from . import tpset_producer_gen
    console.log("Loading tpset_writer config generator")
    from . import tpset_writer_gen
    console.log(f"Generating configs")

    network_endpoints={
        "tpset1" : "tcp://{host_producer}:12344",
        "tpset2" : "tcp://{host_producer}:12345",
    }
    
    cmd_data_producer = tpset_producer_gen.generate(
        INPUT_FILE = input_file,
        SLOWDOWN_FACTOR = slowdown_factor,
        NETWORK_ENDPOINTS = network_endpoints
    )

    cmd_data_writer = tpset_writer_gen.generate(
        NETWORK_ENDPOINTS = network_endpoints
    )

    if exists(json_dir):
        raise RuntimeError(f"Directory {json_dir} already exists")

    data_dir = join(json_dir, 'data')
    os.makedirs(data_dir)

    app_producer="tpset_producer"
    app_writer="tpset_writer"

    jf_producer = join(data_dir, app_producer)
    jf_writer = join(data_dir, app_writer)

    cmd_set = ["init", "conf", "start", "stop", "pause", "resume", "scrap"]
    for app,data in ((app_producer, cmd_data_producer),
                     (app_writer, cmd_data_writer),):
        console.log(f"Generating {app} command data json files")
        for c in cmd_set:
            with open(f'{join(data_dir, app)}_{c}.json', 'w') as f:
                json.dump(data[c].pod(), f, indent=4, sort_keys=True)


    console.log(f"Generating top-level command json files")
    start_order = [app_writer, app_producer]
    for c in cmd_set:
        with open(join(json_dir,f'{c}.json'), 'w') as f:
            cfg = {
                "apps": { app: f'data/{app}_{c}' for app in (app_producer, app_writer) }
            }
            if c == 'start':
                cfg['order'] = start_order
            elif c == 'stop':
                cfg['order'] = start_order[::-1]

            json.dump(cfg, f, indent=4, sort_keys=True)


    console.log(f"Generating boot json file")
    with open(join(json_dir,'boot.json'), 'w') as f:
        cfg = generate_boot(
            producer_spec = {
                "name": app_producer,
                "host": "localhost",
                "port": 3333
            },
            writer_spec = {
                "name": app_writer,
                "host": "localhost",
                "port": 3334
            },
        )
        json.dump(cfg, f, indent=4, sort_keys=True)
    console.log(f"config generated in {json_dir}")


if __name__ == '__main__':

    try:
            cli(show_default=True, standalone_mode=True)
    except Exception as e:
            console.print_exception()
