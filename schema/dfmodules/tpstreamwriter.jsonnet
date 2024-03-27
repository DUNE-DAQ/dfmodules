local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.tpstreamwriter";
local s = moo.oschema.schema(ns);

local types = {
    size: s.number("Size", "u8", doc="A count of very many things"),

    dsparams: s.any("DataStoreParams", doc="Parameters that configure a data store"),

    sourceid_number : s.number("sourceid_number", "u4", doc="Source identifier"),

    float : s.number("Float", "f4", doc="A floating point number of 4 bytes"),

    conf: s.record("ConfParams", [
        s.field("tp_accumulation_interval_ticks", self.size, 62500000,
                doc="Size of the TP accumulation window, measured in clock ticks"),
        s.field("tp_accumulation_inactivity_time_before_write_sec", self.float, 1.0,
                doc="Amount of time in which there must be no new data before a given time slice is written out (default is 1 sec)"),
        s.field("data_store_parameters", self.dsparams,
                doc="Parameters that configure the DataStore associated with this TPStreamWriter"),
        s.field("source_id", self.sourceid_number, 999, doc="Source ID of TPSW instance, added to time slice header"),
    ], doc="TPStreamWriter configuration parameters"),

};

moo.oschema.sort_select(types, ns)
