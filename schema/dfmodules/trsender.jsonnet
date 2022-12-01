local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.trsender";
local s = moo.oschema.schema(ns);

local types = {

 //   int4 :    s.number(  "int4",    "i4",          doc="A signed integer of 4 bytes"),
 //   uint4 :   s.number(  "uint4",   "u4",          doc="An unsigned integer of 4 bytes"),
    count:    s.number(  "Count",    "i8",          doc="A signed integer of 8 bytes"),
 //   uint8 :   s.number(  "uint8",   "u8",          doc="An unsigned integer of 8 bytes"),
 //   float4 :  s.number(  "float4",  "f4",          doc="A float of 4 bytes"),
 //   double8 : s.number(  "double8", "f8",          doc="A double of 8 bytes"),
 //   boolean:  s.boolean( "Boolean",                doc="A boolean"),
    string:   s.string(  "String",   		   doc="A string"),   
  
    conf: s.record("Conf", [
                           s.field("runNumber", self.count, 13,
                                           doc="Run number"),                                           
                           s.field("triggerCount", self.count, 1,
                                           doc="Number of the trigger record"),
                           s.field("dataSize", self.count, 1000,
                                           doc="Size of the data - fragment size without the size of its header"),
                           s.field("stypeToUse", self.string, "Detector_Readout",
                                           doc="Subsystem type"),
                           s.field("dtypeToUse", self.string, "HD_TPC",
                                           doc="Subdetector type"),
                           s.field("ftypeToUse", self.string, "WIB",
                                           doc="Fragment type"),
                           s.field("elementCount", self.count, 10,
                                           doc="Number of fragments in trigger record"),
                           s.field("waitBetweenSends", self.count, 100,
                                           doc="Number of milliseconds to wait between sends")
                           ],
                   doc="TrSender configuration"),

};

moo.oschema.sort_select(types, ns)
