local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.fragmentreceiver";
local s = moo.oschema.schema(ns);

local types = {

    timestamp_diff: s.number( "TimestampDiff", "i8", 
    		              doc="A timestamp difference" ),

    queue_timeout: s.number( "QueueTimeout", "u4", 
                             doc="Queue timeout in milliseconds" ),				    

    conf: s.record("ConfParams", [
	s.field("general_queue_timeout", self.queue_timeout, 100, 
                doc="General indication for timeout"),
        s.field("max_timestamp_diff", self.timestamp_diff, 50000000, 
                doc="General indication for timeout to throw errors"),	       		
    ], doc="Fragment Receiver configuration"),

};

moo.oschema.sort_select(types, ns)
