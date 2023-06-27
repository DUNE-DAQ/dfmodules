# 27-Jun-2023, KAB & ELF: notes on existing integrationtests...

Here is a sample command for invoking a test (feel free to keep or drop the options in brackets, as you prefer):

```
pytest -s minimal_system_quick_test.py [--nanorc-option partition-number 3] [--nanorc-option timeout 300]
```

For reference, here are the ideas behind the existing tests:
* minimal_system_quick_test.py - verify that a small emulator system works fine and data gets written in a short run
* 3ru_3df_multirun_test.py - verify that a system with multiple RUs and multiple DF Apps works as expected, including TPG
* fake_data_producer_test.py - verify that the FakeDataProd DAQModule works as expected
* long_window_readout_test.py - verify that readout windows that require TriggerRecords to be split into multiple sequences works as expected
* large_trigger_record_test.py - verify that TriggerRecords that are close to the size of a whole file get written to disk correctly
* 3ru_1df_multirun_test.py - verify that we don't get empty fragments at end run
  * this test is also useful in looking into high-CPU-usage scenarios because it has 3 RUs
* tpstream_writing_test.py - verify that TPSets are written to the TP-stream file(s)
* disabled_output_test.py - verify that the --disable-data-storage option works
* multi_output_file_test.py - test that the file size maximum config parameter works
* insufficient_disk_space_test.py - verify that the appropriate errors and warnings are produced when there isn't enough disk space to write data
