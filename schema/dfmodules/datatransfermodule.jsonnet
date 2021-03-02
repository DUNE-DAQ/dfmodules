local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.datatransfermodule";
local s = moo.oschema.schema(ns);

local types = {
    size: s.number("Size", "u8",
                   doc="A count of very many things"),

    count : s.number("Count", "i4",
                     doc="A count of not too many things"),

    dsparams: s.any("DataStoreParams", doc="Parameters that configure a data store"),

    conf: s.record("ConfParams", [
        s.field("sleep_msec_while_running", self.count, 1000,
                doc="Millisecs to sleep between generating data"),
        s.field("input_data_store_parameters", self.dsparams,
                doc="Parameters that configure the DataStore instance from which data is read"),
        s.field("output_data_store_parameters", self.dsparams,
                doc="Parameters that configure the DataStore instance into which data is written"),
    ], doc="DataTransferModule configuration"),

};

moo.oschema.sort_select(types, ns)
