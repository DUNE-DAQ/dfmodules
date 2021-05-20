// This is the application info schema used by the data writer module.
// It describes the information object structure passed by the application 
// for operational monitoring

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.dfmodules.datawriterinfo");

local info = {
   cl : s.string("class_s", moo.re.ident, doc="A string field"), 
   uint8  : s.number("uint8", "u8", doc="An unsigned of 8 bytes"),

   info: s.record("Info", [
       s.field("class_name", self.cl, "datawriterinfo", doc="Info class name"),
       s.field("records_received", self.uint8, 0, doc="Integral trigger records received counter"), 
       s.field("new_records_received", self.uint8, 0, doc="Incremental trigger records received counter"), 
       s.field("records_written", self.uint8, 0, doc="Integral trigger records written counter"), 
       s.field("new_records_written", self.uint8, 0, doc="Incremental trigger records written counter"), 
       s.field("bytes_output", self.uint8, 0, doc="Number of bytes that have been written out"), 
   ], doc="Data writer information")
};

moo.oschema.sort_select(info) 
