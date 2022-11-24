// This is the configuration schema for dfmodules

local moo = import "moo.jsonnet";
local sdc = import "daqconf/confgen.jsonnet";
local daqconf = moo.oschema.hier(sdc).dunedaq.daqconf.confgen;

local ns = "dunedaq.dfmodules.confgen";
local s = moo.oschema.schema(ns);

local cs = {
 //   int4 :    s.number(  "int4",    "i4",          doc="A signed integer of 4 bytes"),
 //   uint4 :   s.number(  "uint4",   "u4",          doc="An unsigned integer of 4 bytes"),
    int8 :    s.number(  "int8",    "i8",          doc="A signed integer of 8 bytes"),
 //   uint8 :   s.number(  "uint8",   "u8",          doc="An unsigned integer of 8 bytes"),
 //   float4 :  s.number(  "float4",  "f4",          doc="A float of 4 bytes"),
 //   double8 : s.number(  "double8", "f8",          doc="A double of 8 bytes"),
 //   boolean:  s.boolean( "Boolean",                doc="A boolean"),
    string:   s.string(  "String",   		   doc="A string"),   
 //   monitoring_dest: s.enum(     "MonitoringDest", ["local", "cern", "pocket"]),

  dfmodules: s.record("dfmodules", [
    s.field('name_string', self.string, default="Name", doc='Name parameter'),
    s.field("run_number", self.int8, default=1234, doc="Run number"),
    s.field("trigger_count", self.int8, default=1, doc="Number of the trigger record"),
    s.field("data_size", self.int8, default=1000, doc="Size of the data - fragment size without the size of its header"),
    s.field("stype_to_use", self.string, default="Detector_Readout", doc="Subsystem type"),
    s.field("dtype_to_use", self.string, default="HD_TPC", doc="Subdetector type"),
    s.field("ftype_to_use", self.string, default="WIB", doc="Fragment type"),
    s.field("element_count", self.int8, default=10, doc="Number of fragments in trigger record"),
    s.field("wait_ms", self.int8, default=100, doc="Number of milliseconds to wait between sends"),
    ]),

    sender_gen: s.record("sender_gen", [
        s.field("boot", daqconf.boot, default=daqconf.boot, doc="Boot parameters"),
        s.field("dfmodules", self.dfmodules, default=self.dfmodules, doc="dfmodules parameters"),
    ]),
};

// Output a topologically sorted array.
sdc + moo.oschema.sort_select(cs, ns)
