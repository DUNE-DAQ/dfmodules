local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.datawriter";
local s = moo.oschema.schema(ns);

local types = {
    count : s.number("Count", "i4",
                     doc="A count of not too many things"),

    dsparams: s.any("DataStoreParams", doc="Parameters that configure a data store"),

    conf: s.record("ConfParams", [
        s.field("threshold_for_inhibit", self.count, "5",
                doc="Threshold (for number of triggers being processed) for generating a Trigger Inhibit"),
        s.field("data_store_parameters", self.dsparams,
                doc="Parameters that configure the DataStore associated with this DataWriter"),
    ], doc="DataWriter configuration"),

};

moo.oschema.sort_select(types, ns)
