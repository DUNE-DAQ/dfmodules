# This module facilitates the generation of dfmodules DAQModules within dfmodules apps


# Set moo schema search path                                                                              
from dunedaq.env import get_moo_model_path
import moo.io
moo.io.default_load_path = get_moo_model_path()

# Load configuration types                                                                                
import moo.otypes
moo.otypes.load_types("dfmodules/trsender.jsonnet")

import dunedaq.dfmodules.trsender as trsender

from daqconf.core.app import App, ModuleGraph
from daqconf.core.daqmodule import DAQModule
#from daqconf.core.conf_utils import Endpoint, Direction

def get_sender_app(nickname, number_of_run = 1234, index_of_file = 0, number_of_trigger = 1, size_of_data = 1000, subsystem_type = "Recorded_data",
        subdetector_type = "HD_TPC", fragment_type = "WIB", number_of_elements = 10, n_wait_ms=100, host = "localhost"):
    """
    Here the configuration for an entire daq_application instance using DAQModules from dfmodules is generated.
    """

    modules = []
    modules += [DAQModule(name="ts", plugin="TrSender", conf=trsender.Conf(runNumber=number_of_run, triggerCount = number_of_trigger,
     dataSize = size_of_data, stypeToUse=subsystem_type, dtypeToUse = subdetector_type, ftypeToUse=fragment_type,
      elementCount = number_of_elements, waitBetweenSends = n_wait_ms))]
    modules += [DAQModule(name="r", plugin="Receiver")]


    mgraph = ModuleGraph(modules)
    mgraph.connect_modules("ts.trigger_record_output", "r.trigger_record_input", "trigger_record", 10)
    sender_app = App(modulegraph = mgraph, host = host, name = nickname)

    return sender_app
