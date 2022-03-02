local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.tpstreamwriter";
local s = moo.oschema.schema(ns);

local types = {
    size: s.number("Size", "u8", doc="A count of very many things"),

    conf: s.record("ConfParams", [
        s.field("max_file_size_bytes", self.size, 1000000000,
                doc="Maximum number of bytes in each raw data file"),
    ], doc="TPStreamWriter configuration parameters"),

};

moo.oschema.sort_select(types, ns)
