* 29-Apr-2025, KAB, ELF, and others: notes on existing integtests

"integtests" are intended to be automated integration and/or system tests that make use of the
"pytest" framework to validate the operation of the DAQ system in various scenarios.

Here is a sample command for invoking a test (feel free to keep or drop the options in brackets, as you prefer):

```
pytest -s max_file_size_test.py [--nanorc-option log-level debug]  # this nanorc option is still useful even when using drunc
```

For reference, here are the ideas behind the existing tests:
* `max_file_size_test.py` - verifies that data files are closed when they reach a specified maximum file size (approximately)
* `multiple_data_writers_test.py` - verifies that we can run multiple DataWriters for a single TriggerRecordBuilder
* `hdf5_compression_test.py` - verifies that HDF5 compression is working as expected by writing several data files of known size

* `large_trigger_record_test.py` - verify that TriggerRecords that are close to the size of a whole file get written to disk correctly
* `disabled_output_test.py` - verify that the --disable-data-storage option works
* `insufficient_disk_space_test.py` - verify that the appropriate errors and warnings are produced when there isn't enough disk space to write data
