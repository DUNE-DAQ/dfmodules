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

moo.otypes.load_types('nwqueueadapters/networktoqueue.jsonnet')
moo.otypes.load_types('nwqueueadapters/networkobjectreceiver.jsonnet')
moo.otypes.load_types('trigger/triggerprimitivemaker.jsonnet')


# Import new types
import dunedaq.cmdlib.cmd as basecmd # AddressedCmd, 
import dunedaq.rcif.cmd as rccmd # AddressedCmd, 
import dunedaq.appfwk.cmd as cmd # AddressedCmd, 
import dunedaq.appfwk.app as app # AddressedCmd,

import dunedaq.nwqueueadapters.networktoqueue as ntoq
import dunedaq.nwqueueadapters.networkobjectreceiver as nor

from appfwk.utils import acmd, mcmd, mrccmd, mspec

import json
import math
from pprint import pprint


#===============================================================================
def generate(
        NETWORK_ENDPOINTS: dict
):
    cmd_data = {}

    # Define modules and queues
    queue_specs = [
        app.QueueSpec(inst="tpset_q", kind='FollySPSCQueue', capacity=10000)
    ]

    mod_specs = [
        mspec("tps_writer", "TPSetWriter",
              [app.QueueInfo(name="tpset_source", inst="tpset_q", dir="input")]
              ),

        mspec("ntoq", "NetworkToQueue",
              [app.QueueInfo(name="output", inst="tpset_q", dir="output")]
              )
    ]

    cmd_data['init'] = app.Init(queues=queue_specs, modules=mod_specs)

    cmd_data['conf'] = acmd([
        ("ntoq", ntoq.Conf(msg_type="dunedaq::trigger::TPSet",
                           msg_module_name="TPSetNQ",
                           receiver_config=nor.Conf(ipm_plugin_type="ZmqSubscriber",
                                                    address=NETWORK_ENDPOINTS["tpset"],
                                                    subscriptions=["foo"]
                                                    ) # Empty subscription means subscribe to everything
                           )
         )

    ])

    startpars = rccmd.StartParams(run=1, disable_data_storage=False)
    cmd_data['start'] = acmd([
        ("ntoq", startpars),
        ("tps_writer", startpars),
    ])

    cmd_data['pause'] = acmd([])

    cmd_data['resume'] = acmd([])
    
    cmd_data['stop'] = acmd([
        ("tps_writer", None),
        ("ntoq", None),
    ])

    cmd_data['scrap'] = acmd([
    #     ("tpm", None),
    ])

    return cmd_data
