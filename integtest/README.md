* 19-Jul-2023, KAB, ELF, and others: notes on existing integtests

"integtests" are intended to be automated integration and/or system tests that make use of the
"pytest" framework to validate the operation of the DAQ system in various scenarios.

Here is a sample command for invoking a test (feel free to keep or drop the options in brackets, as you prefer):

```
pytest -s disabled_output_test.py [--nanorc-option partition-number 2] [--nanorc-option timeout 300]
```

For reference, here are the ideas behind the existing tests:
* `large_trigger_record_test.py` - verify that TriggerRecords that are close to the size of a whole file get written to disk correctly
* `disabled_output_test.py` - verify that the --disable-data-storage option works
* `multi_output_file_test.py` - test that the file size maximum config parameter works
* `insufficient_disk_space_test.py` - verify that the appropriate errors and warnings are produced when there isn't enough disk space to write data
