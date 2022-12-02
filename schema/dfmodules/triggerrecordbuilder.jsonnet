local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.triggerrecordbuilder";
local s = moo.oschema.schema(ns);

local types = {
    sourceid_number : s.number("sourceid_number", "u4",
                     doc="Source identifier"),

    connection_id : s.string("connection_id", doc="Connection Name to be used with NetworkManager"),

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
                                   s.field("reply_connection_name", self.connection_id, "nwmgr_test.frags_0",
				   	   doc="" ),
                                   s.field("source_id", self.sourceid_number, doc="Source ID of TRB instance, added to trigger record header"),
                                  ] , 
                   doc="TriggerRecordBuilder configuration")

};

moo.oschema.sort_select(types, ns)
