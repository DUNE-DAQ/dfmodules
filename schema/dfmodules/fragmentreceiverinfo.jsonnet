// This is the application info schema used by the fragment receiver module.
// It describes the information object structure passed by the application 
// for operational monitoring

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.dfmodules.fragmentreceiverinfo");

local info = {
   cl : s.string("class_s", moo.re.ident,
                  doc="A string field"), 
    uint8  : s.number("uint8", "u8",
                     doc="An unsigned of 8 bytes"),

   info: s.record("Info", [
       s.field("class_name", self.cl, "fragmentreceiverinfo", doc="Info class name"),
       s.field("trigger_decisions", self.uint8, 0, doc="Number of trigger decisions in the book"), 
       s.field("trigger_fragments", self.uint8, 0, doc="number of trigger decisions with at least one fragment"), 
       s.field("total_fragments", self.uint8, 0, doc="Total number of fragments in the book")       
   ], doc="Fragment receiver information")
};

moo.oschema.sort_select(info) 
