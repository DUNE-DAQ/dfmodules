// This is the application info schema used by the fake data producer.
// It describes the information object structure passed by the application
// for operational monitoring

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.dfmodules.datafloworchestratorinfo");

local info = {
   uint8  : s.number("uint8", "u8", doc="An unsigned of 8 bytes"),

   info: s.record("Info", [
       s.field("tokens_received", self.uint8, 0, doc="Number of received tokens"),
       s.field("decisions_received", self.uint8, 0, doc="Number of trigger decisions obtained from trigger"),
       s.field("decisions_sent", self.uint8, 0, doc="Number of sent trigger decisions"),
       s.field("waiting_for_decision", self.uint8, 0, doc="Time spent waiting on Trigger Decisions"),
       s.field("deciding_destination", self.uint8, 0, doc="Time spent making a decision on the receving DF app"),
       s.field("forwarding_decision", self.uint8, 0, doc="Time spent sending the Trigger Decision to TRB"),
       s.field("waiting_for_token", self.uint8, 0, doc="Time spent waiting in token thread for tokens"),
       s.field("processing_token", self.uint8, 0, doc="Time spent in token thread updating data structure"),
       s.field("average_time_since_assignment", self.uint8, 0, doc="average time since assignment for current TDs (ms)"),
       s.field("min_time_since_assignment", self.uint8, 0, doc="shortest time since assignment among current TDs (ms)"),
       s.field("max_time_since_assignment", self.uint8, 0, doc="longest time since assignment among current TDs (ms)")
   ], doc="Data Flow Orchestrator information")
};

moo.oschema.sort_select(info)