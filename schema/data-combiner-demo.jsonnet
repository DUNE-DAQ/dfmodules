// Example commands for a ddpdemo job.
//
local moo = import "moo.jsonnet";
local cmd = import "appfwk-cmd-make.jsonnet";
local datacombiner = import "ddpdemo-DataCombiner-make.jsonnet";

[
    cmd.init([], [cmd.mspec("dataxfer", "DataTransferModule")]) { waitms: 1000 },

    cmd.conf([cmd.mcmd("dataxfer", datacombiner.conf(1000,
                       "HDF5DataStore","input_store",".","demo_run20201104","one-fragment-per-file",
                       "HDF5DataStore","output_store",".","demo_run20201104","one-event-per-file"))])
        { waitms: 1000 },

    cmd.start(42) { waitms: 1000 },

    cmd.stop() { waitms: 1000 },
    
    ### there is already a version of the appfwk in which waitms is no processed anymore
]
