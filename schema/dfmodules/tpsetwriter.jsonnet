local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.tpsetwriter";
local s = moo.oschema.schema(ns);

local types = {
    a_number : s.number("a_number", "u2", doc="placeholder"),

    conf: s.record("ConfParams", [
        s.field("sample_number", self.a_number, 0, doc=""),
    ], doc="TPSetWriter configuration parameters"),

};

moo.oschema.sort_select(types, ns)
