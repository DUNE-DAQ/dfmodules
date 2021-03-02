local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.datagenerator";
local s = moo.oschema.schema(ns);

local types = {
    size: s.number("Size", "u8",
                   doc="A count of very many things"),

    count : s.number("Count", "i4",
                     doc="A count of not too many things"),

    dsparams: s.any("DataStoreParams", doc="Parameters that configure a data store"),

    conf: s.record("ConfParams", [
        s.field("geo_location_count", self.count, 4,
                doc="Number of geographic subdivisions within the detector"),
        s.field("io_size", self.size, 1048576,
                doc="Size of data buffer to generate (in bytes)"),
        s.field("sleep_msec_while_running", self.count, 1000,
                doc="Millisecs to sleep between generating data"),
        s.field("data_store_parameters", self.dsparams,
                doc="Parameters that configure the DataStore instance"),
    ], doc="DataGenerator configuration"),

};

moo.oschema.sort_select(types, ns)
