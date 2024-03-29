local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.datafloworchestrator";
local s = moo.oschema.schema(ns);

local types = {
    count : s.number("Count", "i4", doc="A count of not too many things"),
    connection_name : s.string("connection_name"),
    timeout: s.number( "Timeout", "u8", 
                       doc="Queue timeout in milliseconds" ),    

    busy_thresholds: s.record("busy_thresholds", [
      s.field( "free", self.count, 5, doc="Maximum number of trigger decisions the application need to be considered free. The values is not considered free (extreme not included)"), 
      s.field( "busy", self.count, 10, doc="Minimum number of trigger decisions the application need to be considered busy. The value is considered busy (extreme included)") 
      ], doc="threshold definitions" ),


    conf: s.record("ConfParams", [
        s.field("general_queue_timeout", self.timeout, 100, 
	        doc="General indication for timeout"),
        s.field("stop_timeout", self.timeout, 10000, 
	        doc="timeout for the stop transition of the DFO to allow collection of remaining tokens."),
        s.field("td_send_retries", self.count, 5, doc="Number of times to retry sending TriggerDecisions"),
        s.field("thresholds", self.busy_thresholds, doc="Watermark controls")
    ], doc="DataFlowOchestrator configuration parameters"),

};

moo.oschema.sort_select(types, ns)
