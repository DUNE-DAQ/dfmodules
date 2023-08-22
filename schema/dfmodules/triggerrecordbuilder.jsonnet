local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.triggerrecordbuilder";
local s = moo.oschema.schema(ns);

local types = {
    numeric_value : s.number("NumericValue", "u4", doc="Reasonably sized number"),

    sourceid_subsystem : s.string("SourceIDSubsystem", doc="Name of a SourceID Subsystem"),

    source_id : s.record("SourceID", [
        s.field("subsys", self.sourceid_subsystem, doc="SourceID subsystem"),
        s.field("id", self.numeric_value, 0, doc="SourceID ID"),
    ], doc="A single SourceID"),

    timeout: s.number( "Timeout", "u8", doc="Queue timeout in milliseconds" ),    

    timestamp_diff: s.number( "TimestampDiff", "i8", doc="A timestamp difference" ),

    dro_appname : s.string("DROAppNameString", doc="The name of a DetectorReadout application"),

    sourceid_list : s.sequence("SourceIDList", self.source_id, doc="List of SourceIDs"),

    dro_appname_srcid_entry : s.record("DROAppNameSrcIDEntry", [
        s.field("appname", self.dro_appname, doc="DetectorReadout application name"),
        s.field("srcid_list", self.sourceid_list, doc="Source ID list")
    ], doc="DRO AppName SourceID Map entry"),

    dro_appname_srcid_map : s.sequence("DROAppNameSrcIDMap", self.dro_appname_srcid_entry, doc="DRO appname to SourceID map" ),

    conf: s.record("ConfParams", [ s.field("general_queue_timeout", self.timeout, 100, 
                                           doc="General indication for timeout"),
                                   s.field("trigger_record_timeout_ms", self.timeout, 0, 
                                           doc="Timeout for a TR to be sent incomplete. 0 means no timeout"),
                                   s.field("max_time_window", self.timestamp_diff, 0, 
                                           doc="Maximum time window size for Data requests. 0 means no slicing"),
                                   s.field("source_id", self.numeric_value, doc="Source ID of TRB instance, added to trigger record header"),
                                   s.field("dro_appname_srcid_map", self.dro_appname_srcid_map, doc="The map of readout app names to source IDs"),
                                  ] , 
                   doc="TriggerRecordBuilder configuration")

};

moo.oschema.sort_select(types, ns)
