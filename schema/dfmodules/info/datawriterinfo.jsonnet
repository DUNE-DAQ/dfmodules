// This is the application info schema used by the data writer module.
// It describes the information object structure passed by the application 
// for operational monitoring

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.dfmodules.datawriterinfo");

local info = {
   uint8  : s.number("uint8", "u8", doc="An unsigned of 8 bytes"),

   info: s.record("Info", [
       s.field("records_received", self.uint8, 0, doc="Integral trigger records received counter"), 
       s.field("new_records_received", self.uint8, 0, doc="Incremental trigger records received counter"), 
       s.field("records_written", self.uint8, 0, doc="Integral trigger records written counter"), 
       s.field("new_records_written", self.uint8, 0, doc="Incremental trigger records written counter"), 
       s.field("bytes_output", self.uint8, 0, doc="Number of bytes that have been written out"), 
       s.field("new_bytes_output", self.uint8, 0, doc="incremental bytes that have been written out"),
       s.field("writing_time", self.uint8, 0, doc="Time spent writing (ms)")
   ], doc="Data writer information")
};

moo.oschema.sort_select(info) 
