local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.triggerrecordbuilder";
local s = moo.oschema.schema(ns);

local types = {
    region_number : s.number("region_number", "u2",
                     doc="Region container type for GeoID"),	
    element_number : s.number("element_number", "u4",
                     doc="Element container type for GeoID"),

    queueid : s.string("queue_id", doc="Parameter that configure TriggerRecordBuilder's queues"),
    system_type : s.string("system_type", doc="Parameter that configure TriggerRecordBuilder"),

    geoidqueue : s.record("geoidinst", [s.field("region", self.region_number, doc="" ) , 
                                        s.field("element", self.element_number, doc="" ), 
                                        s.field("system", self.system_type, doc="" ),
                                        s.field("queueinstance", self.queueid, doc="" ) ], 
                           doc="TriggerRecordBuilder configuration"),

    mapgeoidqueue : s.sequence("mapgeoidqueue",  self.geoidqueue, doc="Map of geoids queues" ),

    timestamp_diff: s.number( "TimestampDiff", "i8", 
                              doc="A timestamp difference" ),

    queue_timeout: s.number( "QueueTimeout", "u4", 
                             doc="Queue timeout in milliseconds" ),    
  
    conf: s.record("ConfParams", [ s.field("map", self.mapgeoidqueue, doc="" ), 
                                   s.field("general_queue_timeout", self.queue_timeout, 100, 
                                           doc="General indication for timeout"),
                                   s.field("old_timestamp_diff", self.timestamp_diff, 50000000, 
                                           doc="General time indication for a Trigger record to be considered old"),
                                   s.field("timout_timestamp_diff", self.timestamp_diff, 0, 
                                           doc="Timeout for a TR to be sent incomplete. 0 means no timeout")
                                  ] , 
                   doc="TriggerRecordBuilder configuration")

};

moo.oschema.sort_select(types, ns)
