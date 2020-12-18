local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.faketrigdecemu";
local s = moo.oschema.schema(ns);

local types = {
    count : s.number("Count", "i4",
                     doc="A count of not too many things"),

    conf: s.record("Conf", [
        s.field("sleep_msec_while_running", self.count, 1000,
                doc="Millisecs to sleep between generating data"),
    ], doc="FakeTrigDecEmu configuration"),

};

moo.oschema.sort_select(types, ns)
