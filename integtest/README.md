# 21-Jul-2022, KAB: notes on some initial integrationtests...

Here is a command for fetching a file that has WIB data in it (to be used in generating emulated data).  [Please note that this command assumes that the "DAQ assettools" (`daq-assettools` repo) are available in your software area.  If you are using a relatively recent release or nightly build, this will be the case.]

```
cp -v `assets-list -c 9f14e12a0ebdaf207e9e740044b2433c | awk '{print $NF}'` .
```

Here is a sample command for invoking a test (feel free to keep or drop the options in brackets, as you prefer):

```
pytest -s minimal_system_quick_test.py [--frame-file $PWD/frames.bin] [--nanorc-option partition-number 3] [--nanorc-option timeout 300]
```

For reference, here are the ideas behind the existing tests:
* minimal_system_quick_test.py - verify that a small emulator system works fine and data gets written in a short run
* 3ru_3df_multirun_test.py - verify that a system with multiple DF Apps works as expected
* fake_data_producer_test.py - verify that the FakeDataProd DAQModule works as expected
  * this test does not need "--frame-file $PWD/frames.bin"
* long_window_readout_test.py - verify that readout windows that require TriggerRecords to be split into multiple sequences works as expected
* 3ru_1df_multirun_test.py - verify that we don't get empty fragments at end run
  * this test is also useful in looking into high-CPU-usage scenarios because it has 3 RUs
* tpstream_writing_test.py - verify that TPSets are written to the TP-stream file(s)
* disabled_output_test.py - verify that the --disable-data-storage option works
* multi_output_file_test.py - test that the file size maximum config parameter works

Specialty tests:
* iceberg_real_hsi_test.py - tests the generation of pulser triggers by the real TLU/HSI electronics at the ICEBERG teststand
  * needs to be run on the the iceberg01 computer at the ICEBERG teststand
  * for now, it needs the global timing partition to be started separately (hints provided in output of the test script)
  * this test does not need "--frame-file $PWD/frames.bin"
  * it is useful to run this test with a couple of partition numbers to verify that it can talk to the global timing partition independent of its own partition number
* felix_emu_wib2_test.py - tests the readout of emulated WIB2 data from a real FELIX card
  * requires that the emulated data has already been loaded into the FELIX card (hints provided at the start of the output from the test script)
  * has only been tested at the ICEBERG teststand so far
  * this test does not need "--frame-file $PWD/frames.bin"

Not ready for general use:
* insufficient_disk_space_test.py
* large_trigger_record_test.py
