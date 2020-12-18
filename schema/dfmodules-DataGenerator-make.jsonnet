{
    // Make a conf object for DataGenerator
    conf(geocount=10, iosize=4096, sleepms=1000, dstype="HDF5DataStore", dsname="data_store", dirpath=".", fnprefix="demo_", opmode="all-per-file") :: {
        geo_location_count: geocount,
        io_size: iosize,
        sleep_msec_while_running: sleepms,
        data_store_parameters: {
          name : dsname,
	  type : dstype,
          directory_path: dirpath,
          filename_prefix: fnprefix,
          mode: opmode,
        }
    },
}
