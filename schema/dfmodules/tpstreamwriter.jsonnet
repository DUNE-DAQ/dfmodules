local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.tpstreamwriter";
local s = moo.oschema.schema(ns);

local types = {
    size: s.number("Size", "u8", doc="A count of very many things"),

    dsparams: s.any("DataStoreParams", doc="Parameters that configure a data store"),

    conf: s.record("ConfParams", [
        s.field("tp_accumulation_interval_ticks", self.size, 50000000,
                doc="Size of the TP accumulation window, measured in clock ticks"),
        s.field("data_store_parameters", self.dsparams,
                doc="Parameters that configure the DataStore associated with this TPStreamWriter"),
    ], doc="TPStreamWriter configuration parameters"),

};

moo.oschema.sort_select(types, ns)
