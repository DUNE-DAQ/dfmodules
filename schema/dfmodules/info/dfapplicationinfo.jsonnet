// This is the application info schema used by the fake data producer.
// It describes the information object structure passed by the application
// for operational monitoring

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.dfmodules.dfapplicationinfo");

local info = {
   counter  : s.number("uint8", "u8", doc="An unsigned of 8 bytes"),

   info: s.record("Info", [
       s.field("outstanding_decisions", self.counter, 0, doc="Decisions currently in progress"),	 
       s.field("completed_trigger_records", self.counter, 0, doc="Number of completed TR"),
       s.field("waiting_time", self.counter, 0, doc="cumulative time (microseconds) for decisions to be completed"),
       s.field("total_time_since_assignment", self.uint8, 0, doc="total time since assignment for all current TDs (ms)"),
       s.field("min_time_since_assignment", self.uint8, 0, doc="shortest time since assignment among current TDs (ms)"),
       s.field("max_time_since_assignment", self.uint8, 0, doc="longest time since assignment among current TDs (ms)")

   ], doc="Data Flow Application information")
};

moo.oschema.sort_select(info)