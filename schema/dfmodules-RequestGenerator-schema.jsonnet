local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.requestgenerator";
local s = moo.oschema.schema(ns);

local types = {
    apanumber : s.number("apa_number", "u4",
                     doc="APA number for the mapping of data request queues"),	
    linknumber : s.number("link_number", "u4",
                     doc="Link number for the mapping of data request queues"),

    queueid : s.string("queue_id", doc="Parameter that configure RequestGenerator queue module"),

    geoidqueue : s.record("geoidinst", [s.field("apa", self.apanumber, doc="" ) , s.field("link", self.linknumber, doc="" ), s.field("queueinstance", self.queueid, doc="" ) ], doc="RequestGenerator configuration"),

    mapgeoidqueue : s.sequence("mapgeoidqueue",  self.geoidqueue, doc="Map of geoids queues" ),


    conf: s.record("ConfParams", [s.field("map", self.mapgeoidqueue, doc="" ) ] , doc="RequestGenerator configuration")

};

moo.oschema.sort_select(types, ns)
