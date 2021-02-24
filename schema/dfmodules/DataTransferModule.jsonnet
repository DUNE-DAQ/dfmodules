local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.datatransfermodule";
local s = moo.oschema.schema(ns);

local types = {
    size: s.number("Size", "u8",
                   doc="A count of very many things"),

    count : s.number("Count", "i4",
                     doc="A count of not too many things"),

    dirpath: s.string("DirectoryPath", doc="String used to specify a directory path"),

    fnprefix: s.string("FilenamePrefix", doc="String used to specify a filename prefix"),

    opmode: s.string("OperationMode", doc="String used to specify a data storage operation mode"),

    data_store_name: s.string( "DataStoreName", doc="String to specify names for DataStores"),

    data_store_type: s.string( "DataStoreType", doc="Specific Data store implementation to be instantiated" ),
   

    store: s.record("DataStore", [
        s.field("type", self.data_store_type, "HDF5DataStore", 
                 doc="DataStore specific implementation" ), 
        s.field("name", self.data_store_name, "store", 
                 doc="DataStore name" ), 
        s.field("directory_path", self.dirpath, ".",
                doc="Path of directory where files are located"),
        s.field("filename_prefix", self.fnprefix, "demo_run20201104",
                doc="Filename prefix for the files on disk"),
        s.field("mode", self.opmode, "one-fragment-per-file",
                doc="The operation mode that the DataStore should use when organizing the data into files"),
    ], doc="DataStore configuration"),

    conf: s.record("Conf", [
        s.field("sleep_msec_while_running", self.count, 1000,
                doc="Millisecs to sleep between generating data"),
        s.field("input_data_store_parameters", self.store,
                doc="Parameters that configure the DataStore instance from which data is read"),
        s.field("output_data_store_parameters", self.store,
                doc="Parameters that configure the DataStore instance into which data is written"),
    ], doc="DataTransferModule configuration"),

};

moo.oschema.sort_select(types, ns)
