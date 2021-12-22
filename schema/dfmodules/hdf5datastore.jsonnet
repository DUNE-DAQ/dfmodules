local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.hdf5datastore";
local s = moo.oschema.schema(ns);

local types = {
    size : s.number("Size", "u8", doc="A count of very many things"),

    count : s.number("Count", "i4", doc="A count of not too many things"),

    factor : s.number("Factor", "f4", doc="A float number of 4 bytes"),

    ds_string : s.string("DataStoreString", doc="A string used in the data store configuration"),

    flag: s.boolean("Flag", doc="Parameter that can be used to enable or disable functionality"),

    hdf5_filename_params: s.record("FileNameParams", [
        s.field("overall_prefix", self.ds_string, "minidaq",
                doc="Prefix for the overall filename for the files on disk"),
        s.field("run_number_prefix", self.ds_string, "run",
                doc="Prefix for the run number part of the filename"),
        s.field("digits_for_run_number", self.count, 6,
                doc="Number of digits to use for the run number when formatting the filename"),
        s.field("file_index_prefix", self.ds_string, "",
                doc="Prefix for the file index part of the filename"),
        s.field("digits_for_file_index", self.count, 4,
                doc="Number of digits to use for the file index when formatting the filename"),
    ], doc="Parameters for the HDF5DataStore filenames"),

    subdet_path_params : s.record("PathParams", [
           s.field("detector_group_type", self.ds_string, "unspecified",
                   doc="The special keyword that identifies this entry (e.g. \"TPC\", \"PDS\", \"TPC_TP\", etc.)"),
           s.field("detector_group_name", self.ds_string, "unspecified",
                   doc="The detector name that should be used inside the HDF5 file"),
           s.field("region_name_prefix", self.ds_string, "Region",
                   doc="Prefix for the Region name"),
           s.field("digits_for_region_number", self.count, 3,
                   doc="Number of digits to use for the region number inside the HDF5 file"),
           s.field("element_name_prefix", self.ds_string, "Element",
                   doc="Prefix for the Element name"),
           s.field("digits_for_element_number", self.count, 2,
                   doc="Number of digits to use for the Element number inside the HDF5 file") ],
        doc="Parameters for the HDF5 Group and DataSet names"),

    list_of_path_params : s.sequence("PathParamList", self.subdet_path_params, doc="List of subdetector path parameters" ),

    hdf5_file_layout_params: s.record("FileLayoutParams", [
        s.field("trigger_record_name_prefix", self.ds_string, "TriggerRecord",
                doc="Prefix for the TriggerRecord name"),
        s.field("digits_for_trigger_number", self.count, 6,
                doc="Number of digits to use for the trigger number in the TriggerRecord name inside the HDF5 file"),
        s.field("digits_for_sequence_number", self.count, 4,
                doc="Number of digits to use for the sequence number in the TriggerRecord name inside the HDF5 file"),
        s.field("path_param_list", self.list_of_path_params, doc=""),
    ], doc="Parameters for the layout of Groups and DataSets within the HDF5 file"),

    conf: s.record("ConfParams", [
        s.field("type", self.ds_string, "HDF5DataStore",
                 doc="DataStore specific implementation"),
        s.field("name", self.ds_string, "store",
                 doc="DataStore name"),
        s.field("version", self.count, 1,
                 doc="Version of the HDF5DataStore parameters (e.g. Group and DataSet names)"),
        s.field("operational_environment", self.ds_string, "minidaq",
                 doc="Operational environment (where the DAQ is running, e.g. \"coldbox\")"),
        s.field("mode", self.ds_string, "all-per-file",
                doc="The operation mode that the DataStore should use when organizing the data into files"),
        s.field("directory_path", self.ds_string, ".",
                doc="Path of directory where files are located"),
        s.field("max_file_size_bytes", self.size, 1048576,
                doc="Maximum number of bytes in each raw data file"),
        s.field("disable_unique_filename_suffix", self.flag, 0,
                doc="Flag to disable the addition of a unique suffix to the output filenames"),
        s.field("filename_parameters", self.hdf5_filename_params,
                doc="Parameters that are use for the filenames of the HDF5 files"),
        s.field("file_layout_parameters", self.hdf5_file_layout_params,
                doc="Parameters that are use for the layout of Groups and DataSets within the HDF5 file"),
        s.field("free_space_safety_factor_for_write", self.factor, 5.0,
                doc="The safety factor that should be used when determining if there is sufficient free disk space during write operations"),
    ], doc="HDF5DataStore configuration"),

};

moo.oschema.sort_select(types, ns)
