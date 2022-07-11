local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.requestreceiver";
local s = moo.oschema.schema(ns);

local types = {
    system_type : s.string("system_type", doc="Parameter that configure RequestReceiver"),
    sourceid_number : s.number("sourceid_number", "u4",
                     doc="Source identifier"),

    connectionid : s.string("connection_id", doc="Parameter that configure RequestReceiver's connections"),

    sourceidqueue : s.record("sourceidinst", [s.field("source_id", self.sourceid_number, doc="" ) , 
                                        s.field("system", self.system_type, doc="" ),
                                        s.field("connection_uid", self.connectionid, doc="" ) ], 
                           doc="RequestReceiver configuration"),

    mapsourceidqueue : s.sequence("mapsourceidqueue",  self.sourceidqueue, doc="Map of sourceids queues" ),

    timeout: s.number( "Timeout", "u8", 
                       doc="Queue timeout in milliseconds" ),    
                        
    conf: s.record("ConfParams", [ s.field("map", self.mapsourceidqueue, doc="" ), 
                                   s.field("general_queue_timeout", self.timeout, 100, 
                                           doc="General indication for timeout")
                                  ] , 
                   doc="RequestReceiver configuration")

};

moo.oschema.sort_select(types, ns)
