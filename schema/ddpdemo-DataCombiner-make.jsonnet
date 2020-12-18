{
    // Make a conf object for DataGenerator
    conf(sleepms=1000, in_dstype="HDF5DataStore", in_dsname="data_store", in_dirpath=".", in_fnprefix="demo_", in_opmode="all-per-file",  out_dstype="HDF5DataStore", out_dsname="data_store", out_dirpath=".", out_fnprefix="demo_", out_opmode="all-per-file") :: {
        sleep_msec_while_running: sleepms,
        input_data_store_parameters: {
          name : in_dsname,
	  type : in_dstype,
          directory_path: in_dirpath,
          filename_prefix: in_fnprefix,
          mode: in_opmode,
        },
        output_data_store_parameters: {
          name : out_dsname,
	  type : out_dstype,
          directory_path: out_dirpath,
          filename_prefix: out_fnprefix,
          mode: out_opmode,
        }
    },
}
