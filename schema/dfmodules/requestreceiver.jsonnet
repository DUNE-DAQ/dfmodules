local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.requestreceiver";
local s = moo.oschema.schema(ns);

local types = {
    region_number : s.number("region_number", "u2",
                     doc="Region container type for GeoID"),	
    element_number : s.number("element_number", "u4",
                     doc="Element container type for GeoID"),

    queueid : s.string("queue_id", doc="Parameter that configure RequestReceiver's queues"),
    system_type : s.string("system_type", doc="Parameter that configure RequestReceiver"),

    geoidqueue : s.record("geoidinst", [s.field("region", self.region_number, doc="" ) , 
                                        s.field("element", self.element_number, doc="" ), 
                                        s.field("system", self.system_type, doc="" ),
                                        s.field("queueinstance", self.queueid, doc="" ) ], 
                           doc="RequestReceiver configuration"),

    mapgeoidqueue : s.sequence("mapgeoidqueue",  self.geoidqueue, doc="Map of geoids queues" ),

    timeout: s.number( "Timeout", "u8", 
                       doc="Queue timeout in milliseconds" ),    
    connection_name: s.string("Name", doc="Name for the connection that RequestReceiver listens on"),
                        
    conf: s.record("ConfParams", [ s.field("map", self.mapgeoidqueue, doc="" ), 
                                   s.field("general_queue_timeout", self.timeout, 100, 
                                           doc="General indication for timeout"),
                                   s.field("connection_name", self.connection_name, "", doc="Connection name for listening" )
                                  ] , 
                   doc="RequestReceiver configuration")

};

moo.oschema.sort_select(types, ns)
