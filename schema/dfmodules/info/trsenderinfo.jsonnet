local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.dfmodules.trsenderinfo");

local info = {
    uint8 :   s.number(  "uint8",   "u8",          doc="An unsigned integer of 8 bytes"),

    info: s.record("Info", [
        s.field("trigger_record", self.uint8, 0, doc="Counting sent trigger records"),
        s.field("tr_created", self.uint8, 0, doc="Counting created trigger records"),
        s.field("receive_token", self.uint8, 0, doc="Counting received tokens"),
        s.field("difference", self.uint8, 0, doc="Difference between sent trigger records and received tokens"),
    ], doc="Trigger record sender information"),
};

moo.oschema.sort_select(info)