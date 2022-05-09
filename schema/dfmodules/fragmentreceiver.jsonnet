local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.fragmentreceiver";
local s = moo.oschema.schema(ns);

local types = {
    queueid : s.string("queue_id", doc="Parameter that configure FragmentReceiver's queues"),
   
    timeout: s.number( "Timeout", "u8", 
                       doc="Queue timeout in milliseconds" ),    
                       
    conf: s.record("ConfParams", [  
                                   s.field("general_queue_timeout", self.timeout, 100, 
                                           doc="General indication for timeout"),
                                  ] , 
                   doc="FragmentReceiver configuration")

};

moo.oschema.sort_select(types, ns)
