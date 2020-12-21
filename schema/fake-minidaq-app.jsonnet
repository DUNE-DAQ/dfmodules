local moo = import "moo.jsonnet";
local cmd = import "appfwk-cmd-make.jsonnet";

local NUMBER_OF_FAKE_DATA_PRODUCERS = 3;

local ftde_ns = {
    generate_config_params(sleepms=1000) :: {
        sleep_msec_while_running: sleepms
    },
};

local fdp_ns = {
    generate_config_params(linkno=1) :: {
        temporarily_hacked_link_number: linkno
    },
};

local datawriter_ns = {
    generate_config_params(dirpath=".", opmode="all-per-file") :: {
        directory_path: dirpath,
        mode: opmode
    },
};

local qdict = {
    trigdec_from_ds: cmd.qspec("trigger_decision_from_data_selection", "StdDeQueue", 2),
    triginh_to_ds: cmd.qspec("trigger_inhibit_to_data_selection", "StdDeQueue", 2),
    internal_trigdec_copy: cmd.qspec("trigger_decision_copy_for_bookkeeping", "StdDeQueue", 2),
    trigger_records: cmd.qspec("trigger_records", "StdDeQueue", 2),
} + {
    ["data_requests_"+idx]: cmd.qspec("data_requests_"+idx, "StdDeQueue", 2),
    for idx in std.range(1, NUMBER_OF_FAKE_DATA_PRODUCERS)
} + {
    ["data_fragments_"+idx]: cmd.qspec("data_fragments_"+idx, "StdDeQueue", 2),
    for idx in std.range(1, NUMBER_OF_FAKE_DATA_PRODUCERS)
};

local qspec_list = [
    qdict[xx]
    for xx in std.objectFields(qdict)
];

[
    cmd.init(qspec_list,
             [cmd.mspec("ftde", "FakeTrigDecEmu", [
                  cmd.qinfo("trigger_decision_output_queue", qdict.trigdec_from_ds.inst, "output"),
                  cmd.qinfo("trigger_inhibit_input_queue", qdict.triginh_to_ds.inst, "input")]),
              cmd.mspec("frg", "FakeReqGen", [
                  cmd.qinfo("trigger_decision_input_queue", qdict.trigdec_from_ds.inst, "input"),
                  cmd.qinfo("trigger_inhibit_output_queue", qdict.triginh_to_ds.inst, "output"),
                  cmd.qinfo("trigger_decision_output_queue", qdict.internal_trigdec_copy.inst, "output")] +
                  [cmd.qinfo("data_request_"+idx+"_output_queue", qdict["data_requests_"+idx].inst, "output")
                   for idx in std.range(1, NUMBER_OF_FAKE_DATA_PRODUCERS)
                  ]),
              cmd.mspec("ffr", "FakeFragRec", [
                  cmd.qinfo("trigger_decision_input_queue", qdict.internal_trigdec_copy.inst, "input"),
                  cmd.qinfo("trigger_record_output_queue", qdict.trigger_records.inst, "output")] +
                  [cmd.qinfo("data_fragment_"+idx+"_input_queue", qdict["data_fragments_"+idx].inst, "input")
                   for idx in std.range(1, NUMBER_OF_FAKE_DATA_PRODUCERS)
                  ]),
              cmd.mspec("datawriter", "DataWriter", [
                  cmd.qinfo("trigger_record_input_queue", qdict.trigger_records.inst, "input")])] +
              [cmd.mspec("fdp"+idx, "FakeDataProd", [
                   cmd.qinfo("data_request_input_queue", qdict["data_requests_"+idx].inst, "input"),
                   cmd.qinfo("data_fragment_output_queue", qdict["data_fragments_"+idx].inst, "output")])
               for idx in std.range(1, NUMBER_OF_FAKE_DATA_PRODUCERS)
              ])
              { waitms: 1000 },

    cmd.conf([cmd.mcmd("ftde", ftde_ns.generate_config_params(1000)),
              cmd.mcmd("datawriter", datawriter_ns.generate_config_params(".","all-per-file"))] +
              [cmd.mcmd("fdp"+idx, fdp_ns.generate_config_params(idx))
               for idx in std.range(1, NUMBER_OF_FAKE_DATA_PRODUCERS)
              ]) { waitms: 1000 },

    cmd.start(42) { waitms: 1000 },

    cmd.stop() { waitms: 1000 },
]
