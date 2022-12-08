// This is the configuration schema for dfmodules

local moo = import "moo.jsonnet";
local sdc = import "daqconf/confgen.jsonnet";
local daqconf = moo.oschema.hier(sdc).dunedaq.daqconf.confgen;


local ns = "dunedaq.dfmodules.confgen";
local s = moo.oschema.schema(ns);

local cs = {
    int8 :              s.number("int8",           "i8",   doc="A signed integer of 8 bytes"),
    string:             s.string("String",   		       doc="A string"),   
    count :             s.number("Count",          "i4",   doc="A count of not too many things"),
    connection_name :   s.string("connection_name",        doc="Name of the connection"),
    size :              s.number("Size",           "u8",   doc="A count of very many things"),
    factor :            s.number("Factor",         "f4",   doc="A float number of 4 bytes"),
    ds_string :         s.string("DataStoreString",        doc="A string used in the data store configuration"),
    flag:               s.boolean("Flag",                  doc="Parameter that can be used to enable or disable functionality"),
    hdf_string :        s.string("HDFString",              doc="A string used in the hdf5 configuration"),

  dfmodules: s.record("dfmodules", [
    s.field("w_data_size",                        self.int8,            default=1000,                doc="Size of the data - fragment size without the size of its header"),
    s.field("w_stype_to_use",                     self.string,          default="Detector_Readout",  doc="Subsystem type"),
    s.field("w_dtype_to_use",                     self.string,          default="HD_TPC",            doc="Subdetector type"),
    s.field("w_ftype_to_use",                     self.string,          default="WIB",               doc="Fragment type"),
    s.field("w_element_count",                    self.int8,            default=10,                  doc="Number of fragments in trigger record"),
    s.field("w_wait_ms",                          self.int8,            default=100,                 doc="Number of milliseconds to wait between sends"),
    s.field("w_data_storage_prescale",            self.count,           default=1,                   doc="Prescale value for writing TriggerRecords to storage"),
	s.field("w_min_write_retry_time_usec",        self.count,           default=1000,                doc="The minimum time between retries of data writes, in microseconds"),
    s.field("w_max_write_retry_time_usec",        self.count,           default=1000000,             doc="The maximum time between retries of data writes, in microseconds"),
	s.field("w_write_retry_time_increase_factor", self.count,           default=2,                   doc="The factor that is used to increase the time between subsequent retries of data writes"), 
    s.field("w_decision_connection",              self.connection_name, default="Name",              doc="Connection details to put in tokens for TriggerDecisions"),
    s.field("w_name",                             self.ds_string,       default="store",             doc="DataStore name"),
    s.field("w_operational_environment",          self.ds_string,       default="minidaq",           doc="Operational environment (where the DAQ is running, e.g. \"coldbox\")"),
    s.field("w_mode",                             self.ds_string,       default="all-per-file",      doc="The operation mode that the DataStore should use when organizing the data into files"),
    s.field("w_directory_path",                   self.ds_string,       default=".",                 doc="Path of directory where files are located"),
    s.field("w_max_file_size_bytes",              self.size,            default=1048576,             doc="Maximum number of bytes in each raw data file"),
    s.field("w_disable_unique_filename_suffix",   self.flag,            default=0,                   doc="Flag to disable the addition of a unique suffix to the output filenames"),
    s.field("w_overall_prefix",                   self.ds_string,       default="minidaq",           doc="Prefix for the overall filename for the files on disk"),
    s.field("w_digits_for_run_number",            self.count,           default=6,                   doc="Number of digits to use for the run number when formatting the filename"),
    s.field("w_file_index_prefix",                self.ds_string,       default="",                  doc="Prefix for the file index part of the filename"),
    s.field("w_digits_for_file_index",            self.count,           default=4,                   doc="Number of digits to use for the file index when formatting the filename"),
    s.field("w_writer_identifier",                self.ds_string,       default="",                  doc="String identifying the writer in the filename"),
    s.field("w_record_name_prefix",               self.hdf_string,      default="TriggerRecord",     doc="Prefix for the record name"),
    s.field("w_digits_for_record_number",         self.count,           default=6,                   doc="Number of digits to use for the record number in the record name inside the HDF5 file"),
    s.field("w_hardware_map_file",                self.ds_string,       default="./HardwareMap.txt", doc="The full path to the Hardware Map file that is being used in the current DAQ session"),
    ]),

    sender_gen: s.record("sender_gen", [
    s.field("boot",                               daqconf.boot,         default=daqconf.boot,        doc="Boot parameters"),
    s.field("dfmodules",                          self.dfmodules,       default=self.dfmodules,      doc="dfmodules parameters"),
    ]),
};

// Output a topologically sorted array.
sdc + moo.oschema.sort_select(cs, ns)
