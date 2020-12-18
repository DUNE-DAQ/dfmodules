local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.datawriter";
local s = moo.oschema.schema(ns);

local types = {
    dirpath: s.string("DirectoryPath", doc="String used to specify a directory path"),

    opmode: s.string("OperationMode", doc="String used to specify a data storage operation mode"),

    conf: s.record("ConfParams", [
        s.field("directory_path", self.dirpath, ".",
                doc="Path of directory where files are located"),
        s.field("mode", self.opmode, "all-per-file",
                doc="The operation mode that the DataStore should use when organizing the data into files"),
    ], doc="DataWriter configuration"),

};

moo.oschema.sort_select(types, ns)
