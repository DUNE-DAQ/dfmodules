// This is the configuration schema for dfmodules

local moo = import "moo.jsonnet";
local sdc = import "daqconf/confgen.jsonnet";
local daqconf = moo.oschema.hier(sdc).dunedaq.daqconf.confgen;


local ns = "dunedaq.dfmodules.confgen";
local s = moo.oschema.schema(ns);


//local s_datastore = import "dfmodules/hdf5datastore.jsonnet";
//local datastore = moo.oschema.hier(s_datastore).dunedaq.dfmodules.hdf5datastore;

local cs = {
 //   int4 :    s.number(  "int4",    "i4",          doc="A signed integer of 4 bytes"),
 //   uint4 :   s.number(  "uint4",   "u4",          doc="An unsigned integer of 4 bytes"),
    int8 :    s.number(  "int8",    "i8",          doc="A signed integer of 8 bytes"),
 //   uint8 :   s.number(  "uint8",   "u8",          doc="An unsigned integer of 8 bytes"),
 //   float4 :  s.number(  "float4",  "f4",          doc="A float of 4 bytes"),
 //   double8 : s.number(  "double8", "f8",          doc="A double of 8 bytes"),
 //   boolean:  s.boolean( "Boolean",                doc="A boolean"),
    string:   s.string(  "String",   		   doc="A string"),   
 //   monitoring_dest: s.enum(     "MonitoringDest", ["local", "cern", "pocket"]),
    count : s.number("Count", "i4", doc="A count of not too many things"),
    connection_name : s.string("connection_name", doc="Name of the connection"),
    size : s.number("Size", "u8", doc="A count of very many things"),
    factor : s.number("Factor", "f4", doc="A float number of 4 bytes"),
    ds_string : s.string("DataStoreString", doc="A string used in the data store configuration"),
    flag: s.boolean("Flag", doc="Parameter that can be used to enable or disable functionality"),
    hdf_string : s.string("HDFString", doc="A string used in the hdf5 configuration"),
    dsparams: s.any("DataStoreParams", doc="Parameters that configure a data store"),



  dfmodules: s.record("dfmodules", [
// moje normalni parametry pro trigger record
    s.field('w_name_string', self.string, default="Name", doc='Name parameter'),
    s.field("w_run_number", self.int8, default=13, doc="Run number"),
    s.field("w_trigger_count", self.int8, default=1, doc="Number of the trigger record"),
    s.field("w_data_size", self.int8, default=1000, doc="Size of the data - fragment size without the size of its header"),
    s.field("w_stype_to_use", self.string, default="Detector_Readout", doc="Subsystem type"),
    s.field("w_dtype_to_use", self.string, default="HD_TPC", doc="Subdetector type"),
    s.field("w_ftype_to_use", self.string, default="WIB", doc="Fragment type"),
    s.field("w_element_count", self.int8, default=10, doc="Number of fragments in trigger record"),
    s.field("w_wait_ms", self.int8, default=100, doc="Number of milliseconds to wait between sends"),
// parametry pro wiritng trigger record sebrane z datawriter.jsonnet
        s.field("w_data_storage_prescale", self.count, default=1, doc="Prescale value for writing TriggerRecords to storage"),
//        s.field("w_data_store_parameters", datastore.ConfParams, doc="Parameters that configure the DataStore associated with this DataWriter"),
//        s.field("data_store_parameters", self.dsparams,
//                doc="Parameters that configure the DataStore associated with this DataWriter"),
	    s.field("w_min_write_retry_time_usec", self.count, default=1000, doc="The minimum time between retries of data writes, in microseconds"),
        s.field("w_max_write_retry_time_usec", self.count, default=1000000, doc="The maximum time between retries of data writes, in microseconds"),
	    s.field("w_write_retry_time_increase_factor", self.count, default=2, doc="The factor that is used to increase the time between subsequent retries of data writes"), 
        s.field("w_decision_connection", self.connection_name, default="Name", doc="Connection details to put in tokens for TriggerDecisions"),
/* parametry pro úložiště trigger recordu sebrané z hdf5datastore.jsonnet
    s.field("w_type", self.ds_string, "HDF5DataStore", doc="DataStore specific implementation"),
    s.field("w_name", self.ds_string, "store", doc="DataStore name"),
    s.field("w_operational_environment", self.ds_string, "minidaq", doc="Operational environment (where the DAQ is running, e.g. \"coldbox\")"),
    s.field("w_mode", self.ds_string, "all-per-file", doc="The operation mode that the DataStore should use when organizing the data into files"),
    s.field("w_directory_path", self.ds_string, ".", doc="Path of directory where files are located"),
    s.field("w_max_file_size_bytes", self.size, 1048576, doc="Maximum number of bytes in each raw data file"),
    s.field("w_disable_unique_filename_suffix", self.flag, 0, doc="Flag to disable the addition of a unique suffix to the output filenames"),

hdf5_filename_params: s.record("FileNameParams", [
        s.field("w_overall_prefix", self.ds_string, "minidaq", doc="Prefix for the overall filename for the files on disk"),
        s.field("w_run_number_prefix", self.ds_string, "run", doc="Prefix for the run number part of the filename"),
        s.field("w_digits_for_run_number", self.count, 6, doc="Number of digits to use for the run number when formatting the filename"),
        s.field("w_file_index_prefix", self.ds_string, "", doc="Prefix for the file index part of the filename"),
        s.field("w_digits_for_file_index", self.count, 4, doc="Number of digits to use for the file index when formatting the filename"),
        s.field("w_writer_identifier", self.ds_string, "", doc="String identifying the writer in the filename"),
        s.field("w_trigger_number_prefix", self.ds_string, "", doc="Prefix for the trigger number part of the filename"),
        s.field("w_digits_for_trigger_number", self.count, 6, doc="Number of digits to use for the trigger number when formatting the filename"),


        s.field("w_record_name_prefix", self.hdf_string, "TriggerRecord", doc="Prefix for the record name"),
        s.field("w_digits_for_record_number", self.count, 6, doc="Number of digits to use for the record number in the record name inside the HDF5 file"),
        s.field("w_digits_for_sequence_number", self.count, 4, doc="Number of digits to use for the sequence number in the TriggerRecord name inside the HDF5 file"),
        s.field("w_record_header_dataset_name", self.hdf_string, "TriggerRecordHeader", doc="Dataset name for the record header"),
        s.field("w_raw_data_group_name", self.hdf_string, "RawData", doc="Group name to use for raw data"),
        s.field("w_view_group_name", self.hdf_string, "Views", doc="Group name to use for views of the raw data"),
 //       s.field("path_param_list", self.list_of_path_params, doc=""),
    s.field("w_free_space_safety_factor_for_write", self.factor, 5.0, doc="The safety factor that should be used when determining if there is sufficient free disk space during write operations"),
    s.field("w_hardware_map_file", self.ds_string, "./HardwareMap.txt", doc="The full path to the Hardware Map file that is being used in the current DAQ session"),
// z hdf5libs
        s.field("w_detector_group_type", self.hdf_string, "unspecified", doc="The special keyword that identifies this entry (e.g. \"TPC\", \"PDS\", \"TPC_TP\", etc.)"),
        s.field("w_detector_group_name", self.hdf_string, "unspecified", doc="The detector name that should be used inside the HDF5 file"),
        s.field("w_element_name_prefix", self.hdf_string, "Element", doc="Prefix for the element name"),
        s.field("w_digits_for_element_number", self.count, 5, doc="Number of digits to use for the element ID number inside the HDF5 file"),
*/    ]),

    sender_gen: s.record("sender_gen", [
        s.field("boot", daqconf.boot, default=daqconf.boot, doc="Boot parameters"),
        s.field("dfmodules", self.dfmodules, default=self.dfmodules, doc="dfmodules parameters"),
    ]),
};

// Output a topologically sorted array.
sdc + moo.oschema.sort_select(cs, ns)
