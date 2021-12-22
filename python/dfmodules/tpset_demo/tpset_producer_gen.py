# Set moo schema search path
from dunedaq.env import get_moo_model_path
import moo.io
moo.io.default_load_path = get_moo_model_path()

from pprint import pprint
pprint(moo.io.default_load_path)
# Load configuration types
import moo.otypes
moo.otypes.load_types('rcif/cmd.jsonnet')
moo.otypes.load_types('appfwk/cmd.jsonnet')
moo.otypes.load_types('appfwk/app.jsonnet')

moo.otypes.load_types('nwqueueadapters/queuetonetwork.jsonnet')
moo.otypes.load_types('nwqueueadapters/networkobjectsender.jsonnet')
moo.otypes.load_types('trigger/triggerprimitivemaker.jsonnet')

# Import new types
import dunedaq.cmdlib.cmd as basecmd # AddressedCmd, 
import dunedaq.rcif.cmd as rccmd # AddressedCmd, 
import dunedaq.appfwk.cmd as cmd # AddressedCmd, 
import dunedaq.appfwk.app as app # AddressedCmd,

import dunedaq.nwqueueadapters.queuetonetwork as qton
import dunedaq.nwqueueadapters.networkobjectsender as nos
import dunedaq.trigger.triggerprimitivemaker as tpm

from appfwk.utils import acmd, mcmd, mrccmd, mspec

import json
import math
from pprint import pprint


#===============================================================================
def generate(
        INPUT_FILE: str,
        SLOWDOWN_FACTOR: float,
        NETWORK_ENDPOINTS: dict
):
    cmd_data = {}

    # Derived parameters
    CLOCK_FREQUENCY_HZ = 50000000 / SLOWDOWN_FACTOR

    # Define modules and queues
    queue_specs = [
        app.QueueSpec(inst="tpset_q1", kind='FollySPSCQueue', capacity=1000),
        app.QueueSpec(inst="tpset_q2", kind='FollySPSCQueue', capacity=1000)
    ]

    mod_specs = [
        mspec("tpm1", "TriggerPrimitiveMaker",
              [app.QueueInfo(name="tpset_sink", inst="tpset_q1", dir="output")]
              ),

        mspec("qton1", "QueueToNetwork",
              [app.QueueInfo(name="input", inst="tpset_q1", dir="input")]
              ),

        mspec("tpm2", "TriggerPrimitiveMaker",
              [app.QueueInfo(name="tpset_sink", inst="tpset_q2", dir="output")]
              ),

        mspec("qton2", "QueueToNetwork",
              [app.QueueInfo(name="input", inst="tpset_q2", dir="input")]
              )
    ]

    cmd_data['init'] = app.Init(queues=queue_specs, modules=mod_specs)

    cmd_data['conf'] = acmd([
        ("tpm1", tpm.ConfParams(
            filename=INPUT_FILE,
            number_of_loops=-1, # Infinite
            tpset_time_offset=0,
            tpset_time_width=10000, # 0.2 ms
            region_id=1,
            element_id=2,
            clock_frequency_hz=CLOCK_FREQUENCY_HZ
        )),
        ("qton1", qton.Conf(msg_type="dunedaq::trigger::TPSet",
                            msg_module_name="TPSetNQ",
                            sender_config=nos.Conf(ipm_plugin_type="ZmqPublisher",
                                                   address=NETWORK_ENDPOINTS["tpset1"],
                                                   topic="foo",
                                                   stype="msgpack")
                            )
         ),
        ("tpm2", tpm.ConfParams(
            filename=INPUT_FILE,
            number_of_loops=-1, # Infinite
            tpset_time_offset=0,
            tpset_time_width=10000, # 0.2 ms
            region_id=3,
            element_id=4,
            clock_frequency_hz=CLOCK_FREQUENCY_HZ
        )),
        ("qton2", qton.Conf(msg_type="dunedaq::trigger::TPSet",
                            msg_module_name="TPSetNQ",
                            sender_config=nos.Conf(ipm_plugin_type="ZmqPublisher",
                                                   address=NETWORK_ENDPOINTS["tpset2"],
                                                   topic="foo",
                                                   stype="msgpack")
                            )
         )
    ])

    startpars = rccmd.StartParams(run=1, disable_data_storage=False)
    cmd_data['start'] = acmd([
        ("qton1", startpars),
        ("qton2", startpars),
        ("tpm1", startpars),
        ("tpm2", startpars),
    ])

    cmd_data['pause'] = acmd([])

    cmd_data['resume'] = acmd([])
    
    cmd_data['stop'] = acmd([
        ("tpm2", None),
        ("tpm1", None),
        ("qton2", None),
        ("qton1", None),
    ])

    cmd_data['scrap'] = acmd([
    #     ("tpm", None),
    ])

    return cmd_data
