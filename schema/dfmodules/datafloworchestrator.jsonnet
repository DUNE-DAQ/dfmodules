local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.datafloworchestrator";
local s = moo.oschema.schema(ns);

local types = {
    count : s.number("Count", "i4", doc="A count of not too many things"),
    connection_name : s.string("connection_name"),
    timeout: s.number( "Timeout", "u8", 
                       doc="Queue timeout in milliseconds" ),    

    appconfig: s.record("app_config", [
    s.field("decision_connection", self.connection_name, "", doc="Name of the connection to send decisions to for this application"),
    s.field("capacity", self.count, "5", doc="Number of trigger decisions this application can handle at once")
    ], doc="DataFlow application config"),

    appconfigs: s.sequence("app_configs", self.appconfig, doc="Configuration for the Dataflow applications"),

    conf: s.record("ConfParams", [
        s.field("token_connection", self.connection_name, "", 
	         doc="Connection details to receive job-completed messsages"),	
        s.field("general_queue_timeout", self.timeout, 100, 
	        doc="General indication for timeout"),
        s.field("dataflow_applications", self.appconfigs, doc="Configuration for Dataflow Applications")
    ], doc="DataFlowOchestrator configuration parameters"),

};

moo.oschema.sort_select(types, ns)
