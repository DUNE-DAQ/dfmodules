local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.fakedataprod";
local s = moo.oschema.schema(ns);

local types = {
    count : s.number("Count", "i4",
                     doc="A count of not too many things"),

    conf: s.record("ConfParams", [
        s.field("temporarily_hacked_link_number", self.count, 1,
                doc="Temporary assignement of the FELIX link that is being read out (this will come from upstream soon)"),
    ], doc="FakeDataProd configuration"),

};

moo.oschema.sort_select(types, ns)
