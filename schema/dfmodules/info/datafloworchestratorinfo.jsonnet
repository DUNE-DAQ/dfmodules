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
       s.field("deciding_destination", self.uint8, 0, doc="Time spent making a decision on the receving DF app"),
       s.field("waiting_for_decision", self.uint8, 0, doc="Time spent waiting on Trigger Decisions"),
       s.field("waiting_for_slots", self.uint8, 0, doc="Time spent waiting for new feww slots")
   ], doc="Data Flow Orchestrator information")
};

moo.oschema.sort_select(info)