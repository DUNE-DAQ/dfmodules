local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.requestgenerator";
local s = moo.oschema.schema(ns);

local types = {
    count : s.number("Count", "i4",
                     doc="A count of not too many things"),

    dsparams: s.any("RequestGeneratorParams", doc="Parameters that configure RequestGenerator module"),

    conf: s.record("ConfParams", [ ], doc="RequestGenerator configuration"),

};

moo.oschema.sort_select(types, ns)
