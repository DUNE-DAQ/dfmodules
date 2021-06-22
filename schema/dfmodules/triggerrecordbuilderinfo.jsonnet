// This is the application info schema used by the fragment receiver module.
// It describes the information object structure passed by the application 
// for operational monitoring

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.dfmodules.triggerrecordbuilderinfo");

local info = {
   uint8  : s.number("uint8", "u8", doc="An unsigned of 8 bytes"),
   ratio  : s.number("ratio", "f8", doc="A float number of 8 bytes"),

   info: s.record("Info", [
       s.field("trigger_decisions", self.uint8, 0, doc="Number of trigger decisions in the book"), 
       s.field("fragments", self.uint8, 0, doc="Number of fragments in the book"), 
       s.field("old_fragments", self.uint8, 0, doc="Number of fragments that are late with respect to present time. How late is configurable"),
       s.field("old_trigger_decisions", self.uint8, 0, doc="Number of late fragments"),
       s.field("timed_out_trigger_records", self.uint8, 0, doc="Number of timed out triggers in the run"),
       s.field("sleep_counter", self.uint8, 0, doc="Number times the loop goes to sleep"),
       s.field("average_millisecond_per_trigger", self.ratio, -1, doc="Average time (ms) for a trigger record to be completed")
   ], doc="Trigger Record builder information")
};

moo.oschema.sort_select(info) 
