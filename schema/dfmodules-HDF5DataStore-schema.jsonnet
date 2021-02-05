local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.hdf5datastore";
local s = moo.oschema.schema(ns);

local types = {
    size: s.number("Size", "u8",
                   doc="A count of very many things"),

    count : s.number("Count", "i4",
                     doc="A count of not too many things"),

    data_store_type: s.string( "DataStoreType", doc="Specific Data store implementation to be instantiated" ),

    data_store_name: s.string( "DataStoreName", doc="String to specify names for DataStores"),

    opmode: s.string("OperationMode", doc="String used to specify a data storage operation mode"),

    dirpath: s.string("DirectoryPath", doc="String used to specify a directory path"),

    fnprefix: s.string("FilenamePrefix", doc="String used to specify a filename prefix"),

    flprefix: s.string("FileLayoutPrefix", doc="String used to specify a Group or DataSet name prefix"),

    detname: s.string("DetectorName", doc="String used to specify a detector type"),

    flag: s.boolean("Flag", doc="Parameter that can be used to enable or disable functionality"),

    hdf5_filename_params: s.record("HDF5DataStoreFileNameParams", [
        s.field("overall_prefix", self.fnprefix, "fake_minidaqapp",
                doc="Prefix for the overall filename for the files on disk"),
        s.field("run_number_prefix", self.fnprefix, "run",
                doc="Prefix for the run number part of the filename"),
        s.field("digits_for_run_number", self.count, 6,
                doc="Number of digits to use for the run number when formatting the filename"),
        s.field("file_index_prefix", self.fnprefix, "",
                doc="Prefix for the file index part of the filename"),
        s.field("digits_for_file_index", self.count, 4,
                doc="Number of digits to use for the file index when formatting the filename"),
    ], doc="Parameters for the HDF5DataStore filenames"),

    hdf5_file_layout_params: s.record("HDF5DataStoreFileLayoutParams", [
        s.field("trigger_record_name_prefix", self.flprefix, "TriggerRecord",
                doc="Prefix for the TriggerRecord name"),
        s.field("digits_for_trigger_number", self.count, 6,
                doc="Number of digits to use for the TriggerRecord name inside the HDF5 file"),
        s.field("detector_name", self.detname, "TPC",
                doc="Name for the detector"),
        s.field("apa_name_prefix", self.flprefix, "APA",
                doc="Prefix for the APA name"),
        s.field("digits_for_apa_number", self.count, 3,
                doc="Number of digits to use for the APA number inside the HDF5 file"),
        s.field("link_name_prefix", self.flprefix, "Link",
                doc="Prefix for the Link name"),
        s.field("digits_for_link_number", self.count, 2,
                doc="Number of digits to use for the Link number inside the HDF5 file"),
    ], doc="Parameters for the layout of Groups and DataSets within the HDF5 file"),

    conf: s.record("ConfParams", [
        s.field("type", self.data_store_type, "HDF5DataStore", 
                 doc="DataStore specific implementation" ), 
        s.field("name", self.data_store_name, "store", 
                 doc="DataStore name" ), 
        s.field("mode", self.opmode, "all-per-file",
                doc="The operation mode that the DataStore should use when organizing the data into files"),
        s.field("directory_path", self.dirpath, ".",
                doc="Path of directory where files are located"),
        s.field("max_file_size_bytes", self.size, 1048576,
                doc="Maximum number of bytes in each raw data file"),
        s.field("disable_unique_filename_suffix", self.flag, 0,
                doc="Flag to disable the addition of a unique suffix to the output filenames"),
        s.field("filename_parameters", self.hdf5_filename_params,
                doc="Parameters that are use for the filenames of the HDF5 files"),
        s.field("file_layout_parameters", self.hdf5_file_layout_params,
                doc="Parameters that are use for the layout of Groups and DataSets within the HDF5 file"),
    ], doc="HDF5DataStore configuration"),

};

moo.oschema.sort_select(types, ns)
