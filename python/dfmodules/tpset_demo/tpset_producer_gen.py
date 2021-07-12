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
        app.QueueSpec(inst="tpset_q", kind='FollySPSCQueue', capacity=1000)
    ]

    mod_specs = [
        mspec("tpm", "TriggerPrimitiveMaker",
              [app.QueueInfo(name="tpset_sink", inst="tpset_q", dir="output")]
              ),

        mspec("qton", "QueueToNetwork",
              [app.QueueInfo(name="input", inst="tpset_q", dir="input")]
              )
    ]

    cmd_data['init'] = app.Init(queues=queue_specs, modules=mod_specs)

    cmd_data['conf'] = acmd([
        ("tpm", tpm.ConfParams(
            filename=INPUT_FILE,
            number_of_loops=-1, # Infinite
            tpset_time_offset=0,
            tpset_time_width=10000, # 0.2 ms
            clock_frequency_hz=CLOCK_FREQUENCY_HZ
        )),
        ("qton", qton.Conf(msg_type="dunedaq::trigger::TPSet",
                           msg_module_name="TPSetNQ",
                           sender_config=nos.Conf(ipm_plugin_type="ZmqPublisher",
                                                  address=NETWORK_ENDPOINTS["tpset"],
                                                  topic="foo",
                                                  stype="msgpack")
                           )
         )
    ])

    startpars = rccmd.StartParams(run=1, disable_data_storage=False)
    cmd_data['start'] = acmd([
        ("qton", startpars),
        ("tpm", startpars),
    ])

    cmd_data['pause'] = acmd([])

    cmd_data['resume'] = acmd([])
    
    cmd_data['stop'] = acmd([
        ("tpm", None),
        ("qton", None),
    ])

    cmd_data['scrap'] = acmd([
    #     ("tpm", None),
    ])

    return cmd_data
