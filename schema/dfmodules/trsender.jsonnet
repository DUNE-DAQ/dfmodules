local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.trsender";
local s = moo.oschema.schema(ns);

local types = {
    count:    s.number(  "Count",    "i8",          doc="A signed integer of 8 bytes"),
    string:   s.string(  "String",   		          doc="A string"),   
  
    conf: s.record("Conf", [
                           s.field("dataSize", self.count, 1000,
                                           doc="Size of the data - fragment size without the size of its header"),
                           s.field("stypeToUse", self.string, "Detector_Readout",
                                           doc="Subsystem type"),
                           s.field("dtypeToUse", self.string, "HD_TPC",
                                           doc="Subdetector type"),
                           s.field("ftypeToUse", self.string, "WIB",
                                           doc="Fragment type"),
                           s.field("elementCount", self.count, 10,
                                           doc="Number of fragments in trigger record")
                           ],doc="TrSender configuration"),

};

moo.oschema.sort_select(types, ns)
