local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.fragmentreceiver";
local s = moo.oschema.schema(ns);

local types = {
    queueid : s.string("queue_id", doc="Parameter that configure FragmentReceiver's queues"),
   
    timeout: s.number( "Timeout", "u8", 
                       doc="Queue timeout in milliseconds" ),    
    connection_name: s.string("Name", doc="Name for the connection that FragmentReceiver listens on"),
                        
    conf: s.record("ConfParams", [ s.field("output", self.queueid, doc="" ), 
                                   s.field("general_queue_timeout", self.timeout, 100, 
                                           doc="General indication for timeout"),
                                   s.field("connection_name", self.connection_name, "", doc="Connection name for listening" )
                                  ] , 
                   doc="FragmentReceiver configuration")

};

moo.oschema.sort_select(types, ns)
