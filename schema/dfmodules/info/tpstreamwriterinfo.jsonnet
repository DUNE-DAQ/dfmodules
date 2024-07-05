// This is the application info schema used by the data writer module.
// It describes the information object structure passed by the application 
// for operational monitoring

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.dfmodules.tpstreamwriterinfo");

local info = {
   uint8  : s.number("uint8", "u8", doc="An unsigned of 8 bytes"),

   float4 : s.number("float4", "f4", doc="A float of 4 bytes"),

   info: s.record("Info", [
       s.field("heartbeat_tpsets_received", self.uint8, 0, doc="incremental count of heartbeat TPSets received"),
       s.field("tpsets_with_tps_received", self.uint8, 0, doc="incremental count of TPSets received that contain TPs"),
       s.field("tps_received", self.uint8, 0, doc="incremental count of TPs that have been received"),
       s.field("tps_written", self.uint8, 0, doc="incremental count of TPs that have been written out"),
       s.field("timeslices_written", self.uint8, 0, doc="incremental count of TimeSlices that have been written out"),
       s.field("bytes_output", self.uint8, 0, doc="incremental number of bytes that have been written out"),
       s.field("tardy_timeslice_max_seconds", self.float4, 0, doc="incremental max amount of time that a TimeSlice was tardy"),
   ], doc="TPSet writer information")
};

moo.oschema.sort_select(info) 
