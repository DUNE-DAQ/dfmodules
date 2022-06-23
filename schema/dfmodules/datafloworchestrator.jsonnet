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


    appconfig: s.record("app_config", [
      s.field("connection_uid", self.connection_name, "", doc="Name of the connection to send decisions to for this application"),
      s.field("thresholds", self.busy_thresholds, doc="Watermark controls")
    ], doc="DataFlow application config"),

    appconfigs: s.sequence("app_configs", self.appconfig, doc="Configuration for the Dataflow applications"),

    conf: s.record("ConfParams", [
        s.field("general_queue_timeout", self.timeout, 100, 
	        doc="General indication for timeout"),
        s.field("stop_timeout", self.timeout, 100, 
	        doc="General indication for timeout"),
        s.field("td_send_retries", self.count, 5, doc="Number of times to retry sending TriggerDecisions"),
        s.field("dataflow_applications", self.appconfigs, doc="Configuration for Dataflow Applications")
    ], doc="DataFlowOchestrator configuration parameters"),

};

moo.oschema.sort_select(types, ns)
