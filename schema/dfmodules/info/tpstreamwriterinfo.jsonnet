// This is the application info schema used by the data writer module.
// It describes the information object structure passed by the application 
// for operational monitoring

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.dfmodules.tpstreamwriterinfo");

local info = {
   uint8  : s.number("uint8", "u8", doc="An unsigned of 8 bytes"),

   info: s.record("Info", [
       s.field("tpset_received", self.uint8, 0, doc="incremental received tpset counter"), 
       s.field("tpset_written", self.uint8, 0, doc="incremental written tpset counter"), 
       s.field("bytes_output", self.uint8, 0, doc="incremental number of bytes that have been written out"), 
   ], doc="TPSet writer information")
};

moo.oschema.sort_select(info) 
