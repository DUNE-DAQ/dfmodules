local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.trsender";
local s = moo.oschema.schema(ns);

local types = {
    count:    s.number(  "Count",    "i8",          doc="A signed integer of 8 bytes"),
    string:   s.string(  "String",   		          doc="A string"),   
  
    conf: s.record("Conf", [
                           s.field("dataSize", self.count, 1000,
                                           doc="Size of the data - fragment size without the size of its header"),
                           s.field("hardware_map_file", self.string, "/afs/cern.ch/user/e/eljelink/dunedaq-v3.2.0/sourcecode/dfmodules/scripts/HardwareMap.txt",
                                           doc="The full path to the Hardware Map file that is being used in the current DAQ session"),
                           s.field("m_token_count", self.count, 10,
                                           doc="Difference between sent trigger records and received tokens in trigger record"),
                           ],doc="TrSender configuration"),

};

moo.oschema.sort_select(types, ns)
