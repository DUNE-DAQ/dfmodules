local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.fakedataprod";
local s = moo.oschema.schema(ns);

local types = {
    count : s.number("Count", "u4",
                     doc="A count of not too many things"),
    time : s.number("Time", "u8",
                     doc="A number representing a timestamp"),
    system_type_t : s.string("system_type_t"),
    fragment_type_t : s.string("fragment_type_t"),
    netmgr_name : s.string("NetworkManagerName", doc="Connection or topic name to be used with NetworkManager"),

    conf: s.record("ConfParams", [
        s.field("system_type", self.system_type_t,
                    doc="The system type of the link"),
        s.field("source_id", self.count, 0,
                    doc="The SourceID of this link"),
        s.field("time_tick_diff", self.count, 1,
                    doc="Time tick difference between frames"),
        s.field("frame_size", self.count, 0,
                    doc="The size of a fake frame"),
        s.field("response_delay", self.time, 0,
                    doc="Wait for this amount of ns before sending the fragment"),
        s.field("fragment_type", self.fragment_type_t,
                    doc="Fragment type of the response"),
    ], doc="FakeDataProd configuration"),

};

moo.oschema.sort_select(types, ns)
