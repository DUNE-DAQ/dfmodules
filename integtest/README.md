* 11-Sep-2025, KAB, ELF, and others: notes on existing integtests

"integtests" are intended to be automated integration and/or system tests that make use of the
"pytest" framework to validate the operation of the DAQ system in various scenarios.

Here is a sample command for invoking a test (feel free to keep or drop the options in brackets, as you prefer):

```
pytest -s max_file_size_test.py [--dunerc-option log-level debug]  # this dunerc option is still useful even when using drunc
```

For reference, here are the ideas behind the existing tests:
* `max_file_size_test.py` - verifies that data files are closed when they reach a specified maximum file size (approximately)
* `multiple_data_writers_test.py` - verifies that we can run multiple DataWriters with a single TriggerRecordBuilder
* `hdf5_compression_test.py` - verifies that HDF5 compression is working as expected by writing several data files of known size
* `large_trigger_record_test.py` - verifies that TriggerRecords that have sizes that are 55% and 125% of the configured maximum file size each get written to their own HDF5 file
* `disabled_output_test.py` - verifies that the --disable-data-storage option prevents a raw data file from being produced
* `insufficient_disk_space_test.py` - verifies that the appropriate errors and warnings are produced when there isn't enough disk space to write additional TriggerRecords
* `trmonrequestor_test.py` - verifies that the TriggerRecord monitoring functionality successfully writes a subset of the TRs to disk
* `offline_prod_run_test.py` - verifies that the --run-type=PROD argument to the run control 'start' command has the correct effect in the attributes of the output HDF5 file
