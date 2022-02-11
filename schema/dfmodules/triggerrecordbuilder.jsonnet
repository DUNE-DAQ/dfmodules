local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.triggerrecordbuilder";
local s = moo.oschema.schema(ns);

local types = {
    region_number : s.number("region_number", "u2",
                     doc="Region container type for GeoID"),	
    element_number : s.number("element_number", "u4",
                     doc="Element container type for GeoID"),

    connection_id : s.string("connection_id", doc="Connection Name to be used with NetworkManager"),
    system_type : s.string("system_type", doc="Parameter that configure TriggerRecordBuilder"),

    geoidconnection : s.record("geoidinst", [s.field("region", self.region_number, doc="" ) , 
                                        s.field("element", self.element_number, doc="" ), 
                                        s.field("system", self.system_type, doc="" ),
                                        s.field("connection_name", self.connection_id, doc="" ) ], 
                           doc="TriggerRecordBuilder configuration"),

    mapgeoidconnections : s.sequence("mapgeoidconnections",  self.geoidconnection, doc="Map of geoids queues" ),


    timeout: s.number( "Timeout", "u8", 
                       doc="Queue timeout in milliseconds" ),    

    timestamp_diff: s.number( "TimestampDiff", "i8", 
                              doc="A timestamp difference" ),
 
    conf: s.record("ConfParams", [  s.field("general_queue_timeout", self.timeout, 100, 
                                           doc="General indication for timeout"),
                                   s.field("trigger_record_timeout_ms", self.timeout, 0, 
                                           doc="Timeout for a TR to be sent incomplete. 0 means no timeout"),
                                   s.field("max_time_window", self.timestamp_diff, 0, 
                                           doc="Maximum time window size for Data requests. 0 means no slicing"),
                                   s.field("reply_connection_name", self.connection_id, "nwmgr_test.frags_0", doc="" ),
                                   s.field("mon_connection_name", self.connection_id, "nwmgr_test.trs_0", doc="" ),
                                   s.field("map", self.mapgeoidconnections, doc="" ),
                                  ] , 
                   doc="TriggerRecordBuilder configuration")

};

moo.oschema.sort_select(types, ns)
