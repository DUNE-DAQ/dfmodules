local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.dfmodules.receiverinfo");

local info = {
    uint8  : s.number("uint8", "u8",
        doc="An unsigned of 8 bytes"),

    info: s.record("Info", [
        s.field("trigger_record", self.uint8, 0, doc="Counting received trigger records"),
    ], doc="Receiver information"),

};

moo.oschema.sort_select(info)
