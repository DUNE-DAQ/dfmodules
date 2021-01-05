local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.fragmentreceiver";
local s = moo.oschema.schema(ns);

local types = {

    timestamp_diff: s.number( "TimetampDiff", "i64", doc="A timestamp difference" ),

    decision_loop_counter: s.number( "DecisionLoopCounter", "u8", 
                                     doc="Number of times the trigger decision queue is read before reading the fragment queues" ),

    fragment_loop_counter: s.number( "FragmentLoopCounter", "u8", 
                                     doc="Number of times the fragment queues are read before assessing if some request are complete" ),
				     




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
