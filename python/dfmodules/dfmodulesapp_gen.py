# This module facilitates the generation of dfmodules DAQModules within dfmodules apps


# Set moo schema search path                                                                              
from dunedaq.env import get_moo_model_path
import moo.io
moo.io.default_load_path = get_moo_model_path()

# Load configuration types                                                                                
import moo.otypes
moo.otypes.load_types("dfmodules/trsender.jsonnet")
moo.otypes.load_types("dfmodules/datawriter.jsonnet")
moo.otypes.load_types("dfmodules/hdf5datastore.jsonnet")
moo.otypes.load_types("hdf5libs/hdf5filelayout.jsonnet")

import dunedaq.dfmodules.trsender as trsender
import dunedaq.dfmodules.datawriter as datawriter
import dunedaq.dfmodules.hdf5datastore as hdf5ds
import dunedaq.hdf5libs.hdf5filelayout as h5fl


from daqconf.core.app import App, ModuleGraph
from daqconf.core.daqmodule import DAQModule
#from daqconf.core.conf_utils import Endpoint, Direction

def get_sender_app(nickname, number_of_run = 13, number_of_trigger = 1, size_of_data = 1000, subsystem_type = "Recorded_data",
        subdetector_type = "HD_TPC", fragment_type = "WIB", number_of_elements = 10, n_wait_ms=100, host = "localhost", c_data_storage_prescale = 1,
        c_min_write_retry_time_usec = 1000, c_max_write_retry_time_usec = 1000000, c_write_retry_time_increase_factor = 2, c_decision_connection = "name"):
# c_data_store_parameters


    """
    Here the configuration for an entire daq_application instance using DAQModules from dfmodules is generated.
    """

    modules = []
    modules += [DAQModule(name="ts", plugin="TrSender", conf=trsender.Conf(runNumber=number_of_run, triggerCount = number_of_trigger,
    dataSize = size_of_data, stypeToUse=subsystem_type, dtypeToUse = subdetector_type, ftypeToUse=fragment_type,
    elementCount = number_of_elements, waitBetweenSends = n_wait_ms))]
    modules += [DAQModule(name="dw", plugin="DataWriter", conf=datawriter.ConfParams(data_storage_prescale = c_data_storage_prescale, 
    min_write_retry_time_usec = c_min_write_retry_time_usec, max_write_retry_time_usec = c_max_write_retry_time_usec,
    write_retry_time_increase_factor = c_write_retry_time_increase_factor, decision_connection = c_decision_connection,
    data_store_parameters=hdf5ds.ConfParams(
                               name="data_store",
                               operational_environment = "coldbox",
                               mode = "all-per-file",
                               directory_path = ".",
                               max_file_size_bytes = 4*1024*1024*1024,
                               disable_unique_filename_suffix = False,
                               hardware_map_file="./HardwareMap.txt",
                               filename_parameters = hdf5ds.FileNameParams(
                               overall_prefix = "test",
                               digits_for_run_number = 6,
                               file_index_prefix = "test",
                               digits_for_file_index = 4,
                               writer_identifier = ""),
                               file_layout_parameters = h5fl.FileLayoutParams(
                               record_name_prefix= "TriggerRecord",
                               digits_for_record_number = 5,
                               path_param_list = h5fl.PathParamList(
                                       [h5fl.PathParams(detector_group_type="Detector_Readout",
                                                        detector_group_name="TPC",
                                                        element_name_prefix="Link"),
                                        h5fl.PathParams(detector_group_type="Detector_Readout",
                                                        detector_group_name="PDS"),
                                        h5fl.PathParams(detector_group_type="Detector_Readout",
                                                        detector_group_name="NDLArTPC"),
                                        h5fl.PathParams(detector_group_type="Trigger",
                                                        detector_group_name="DataSelection",
                                                        digits_for_element_number=5),
                                        h5fl.PathParams(detector_group_type="HW_Signals_Interface",
                                                        detector_group_name="HSI")
                                    ])))))]
    
    
  

#  data_store_parameters = c_data_store_parameters

    mgraph = ModuleGraph(modules)
    mgraph.connect_modules("ts.trigger_record_output", "dw.trigger_record_input", "trigger_record", 10)
    mgraph.connect_modules("dw.token_output", "ts.token_input", "token", 10)

    sender_app = App(modulegraph = mgraph, host = host, name = nickname)

    return sender_app
