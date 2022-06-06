# 10-Aug-2021, KAB: notes on some initial integrationtests...

Here is a command for fetching a file that has WIB data in it (to be used in generating emulated data):

* `curl -o frames.bin -O https://cernbox.cern.ch/index.php/s/0XzhExSIMQJUsp0/download`

Here is a sample command for invoking a test:

* `pytest -s minimal_system_quick_test.py --frame-file $PWD/frames.bin`

For reference, here are the ideas behind the existing tests:
* minimal_system_quick_test.py - verify that a small emulator system works fine and data gets written in a short run
* readout_type_scan.py - verify that we can write different types of data (WIB2, PDS, TPG, etc.)
* 3ru_3df_multirun_test.py - verify that a system with multiple DF Apps works as expected
* fake_data_producer_test.py - verify that the FakeDataProd DAQModule works as expected
* long_window_readout_test.py - verify that readout windows that require TriggerRecords to be split into multiple sequences works as expected
* 3ru_1df_multirun_test.py - verify that we don't get empty fragments at end run
  * this test is also useful in looking into high-CPU-usage scenarios because it has 3 RUs
* command_order_test.py - verify that only certain sequences of commands are allowed
* disabled_output_test.py - verify that the --disable-data-storage option works
* multi_output_file_test.py - test that the file size maximum config parameter works
