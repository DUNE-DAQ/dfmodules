local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.dfmodules.trsenderinfo");

local info = {

 //   int4 :    s.number(  "int4",    "i4",          doc="A signed integer of 4 bytes"),
 //   uint4 :   s.number(  "uint4",   "u4",          doc="An unsigned integer of 4 bytes"),
 //   int8 :    s.number(  "int8",    "i8",          doc="A signed integer of 8 bytes"),
    uint8 :   s.number(  "uint8",   "u8",          doc="An unsigned integer of 8 bytes"),
 //   float4 :  s.number(  "float4",  "f4",          doc="A float of 4 bytes"),
 //   double8 : s.number(  "double8", "f8",          doc="A double of 8 bytes"),
 //   boolean:  s.boolean( "Boolean",                doc="A boolean"),
 //   string:   s.string(  "String",                 doc="A string"),   

    info: s.record("Info", [
        s.field("configuration_file", self.uint8, 0, doc="Counting configuration files"),
        s.field("trigger_record", self.uint8, 0, doc="Counting sent trigger records"),
        s.field("tr_created", self.uint8, 0, doc="Counting created trigger records"),
    ], doc="Trigger record sender information"),
};

moo.oschema.sort_select(info)
