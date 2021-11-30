local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.datafloworchestrator";
local s = moo.oschema.schema(ns);

local types = {
    count : s.number("Count", "i4", doc="A count of not too many things"),
    connection_name : s.string("connection_name"),
    timeout: s.number( "Timeout", "u8", 
                       doc="Queue timeout in milliseconds" ),    

    conf: s.record("ConfParams", [
  
        s.field("initial_token_count", self.count, "5",
                doc="Number of tokens to send at the start of the run"),
	s.field("td_connection", self.connection_name, "", 
	         doc="Connection details to send trigger decisions"),
	s.field("token_connection", self.connection_name, "", 
	         doc="Connection details to receive job-completed messsages"),	
        s.field("general_queue_timeout", self.timeout, 100, 
	        doc="General indication for timeout")
    ], doc="DataFlowOchestrator configuration parameters"),

};

moo.oschema.sort_select(types, ns)