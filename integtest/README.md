# 28-Jul-2021, KAB: notes on some initial integrationtests...

Here is the command for fetching an emulated frame data file:

* `curl -o frames.bin -O https://cernbox.cern.ch/index.php/s/VAqNtn7bwuQtff3/download`

Here is a sample command for invoking a test:

* `pytest -s four_process_quick_test.py --frame-file `pwd`/frames.bin`

For reference, here are the ideas behind the existing tests:
* four_process_quick_test.py - verify that data gets written in a short run
* four_process_disabled_output_test.py - verify that the --disable-data-storage option works
* four_process_empty_run_test.py - verify that runs with no events don't crash
* six_process_multi_file_test.py - test that the file size maximum works
* six_process_stop_start_test.py - verify that we don't get empty fragments at end run
