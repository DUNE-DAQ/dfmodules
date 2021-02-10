local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.datawriter";
local s = moo.oschema.schema(ns);

local types = {
    count : s.number("Count", "i4", doc="A count of not too many things"),

    dsparams: s.any("DataStoreParams", doc="Parameters that configure a data store"),

    flag: s.boolean("Flag", doc="Parameter that can be used to enable or disable functionality"),

    run_number: s.number("RunNumber", dtype="u8", doc="Run Number"),

    conf: s.record("ConfParams", [
        s.field("threshold_for_inhibit", self.count, "5",
                doc="Threshold (for number of triggers being processed) for generating a Trigger Inhibit"),
        s.field("data_store_parameters", self.dsparams,
                doc="Parameters that configure the DataStore associated with this DataWriter"),
    ], doc="DataWriter configuration parameters"),

    start: s.record("StartParams", [
        s.field("disable_data_storage", self.flag, 0,
                doc="Flag to disable the storage of data"),
        s.field("data_storage_prescale", self.count, "1",
                doc="Prescale value for writing TriggerRecords to storage"),
        s.field("run", self.run_number, doc="Run Number"),
    ], doc="DataWriter start parameters"),

};

moo.oschema.sort_select(types, ns)
