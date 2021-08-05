// This is the application info schema used by the fragment receiver module.
// It describes the information object structure passed by the application 
// for operational monitoring

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.dfmodules.triggerrecordbuilderinfo");

local info = {
   uint8  : s.number("uint8", "u8", doc="An unsigned of 8 bytes"),
   ratio  : s.number("ratio", "f8", doc="A float number of 8 bytes"),

   info: s.record("Info", [
       s.field("pending_trigger_decisions", self.uint8, 0, doc="Present number of trigger decisions in the book"), 
       s.field("fragments_in_the_book", self.uint8, 0, doc="Present number of fragments in the book"), 
       s.field("pending_fragments", self.uint8, 0, doc="Fragments to be expected based on the TR in the book"), 
       s.field("timed_out_trigger_records", self.uint8, 0, doc="Number of timed out triggers in the run"),
       s.field("unexpected_fragments", self.uint8, 0, doc="Number of unexpected fragments in the run"),
       s.field("unexpected_trigger_decisions", self.uint8, 0, doc="Number of unexpected trigger decisions in the run"),
       s.field("received_trigger_decisions", self.uint8, 0, doc="Number of valid trigger decisions received in the run"),
       s.field("generated_trigger_records", self.uint8, 0, doc="Number of trigger records produced in the run"),
       s.field("abandoned_trigger_records", self.uint8, 0, doc="Number of trigger records that failed to send to writing in the run"),
       s.field("lost_fragments", self.uint8, 0, doc="Number of fragments that not stored in a file in the run"),
       s.field("invalid_requests", self.uint8, 0, doc="Number of requests with unknown GeoID in the run"),
       s.field("duplicated_trigger_ids", self.uint8, 0, doc="Number of TR not created because redundant"),
       s.field("sleep_counter", self.uint8, 0, doc="Number times the loop goes to sleep"),
       s.field("loop_counter", self.uint8, 0, doc="Number times the loop is executed"),
       s.field("average_millisecond_per_trigger", self.ratio, -1, doc="Average time (ms) for a trigger record to be completed"),
       s.field("average_decision_width", self.ratio, -1, doc="Average width (clock ticks) per received trigger decision"),
       s.field("average_data_request_width", self.ratio, -1, doc="Average width (clock ticks) per generated data request")
   ], doc="Trigger Record builder information")
};

moo.oschema.sort_select(info) 
