local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.datawriter";
local s = moo.oschema.schema(ns);

local types = {
    count : s.number("Count", "i4", doc="A count of not too many things"),

    dsparams: s.any("DataStoreParams", doc="Parameters that configure a data store"),

    flag: s.boolean("Flag", doc="Parameter that can be used to enable or disable functionality"),

    run_number: s.number("RunNumber", dtype="u8", doc="Run Number"),

    conf: s.record("ConfParams", [
        s.field("initial_token_count", self.count, "5",
                doc="Number of tokens to send at the start of the run"),
        s.field("data_storage_prescale", self.count, "1",
                doc="Prescale value for writing TriggerRecords to storage"),
        s.field("data_store_parameters", self.dsparams,
                doc="Parameters that configure the DataStore associated with this DataWriter"),
    ], doc="DataWriter configuration parameters"),

};

moo.oschema.sort_select(types, ns)
