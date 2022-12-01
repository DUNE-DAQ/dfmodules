local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.datawriter";
local s = moo.oschema.schema(ns);

local types = {
    count : s.number("Count", "i4", doc="A count of not too many things"),
    connection_name : s.string("connection_name"),
    dsparams: s.any("DataStoreParams", doc="Parameters that configure a data store"),

    conf: s.record("ConfParams", [
        s.field("data_storage_prescale", self.count, "1",
                doc="Prescale value for writing TriggerRecords to storage"),
        s.field("data_store_parameters", self.dsparams,
                doc="Parameters that configure the DataStore associated with this DataWriter"),
	    s.field("min_write_retry_time_usec", self.count, "1000",
	    	doc="The minimum time between retries of data writes, in microseconds"),
	    s.field("max_write_retry_time_usec", self.count, "1000000",
	    	doc="The maximum time between retries of data writes, in microseconds"),
	    s.field("write_retry_time_increase_factor", self.count, "2",
	    	doc="The factor that is used to increase the time between subsequent retries of data writes"),
        s.field("decision_connection", self.connection_name, "", doc="Connection details to put in tokens for TriggerDecisions")
    ], doc="DataWriter configuration parameters"),

};

moo.oschema.sort_select(types, ns)
