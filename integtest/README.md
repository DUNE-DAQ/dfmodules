# 15-Jun-2021, KAB: notes on some initial integrationtests...

I followed the instructions from Phil
(https://github.com/philiprodrigues/integrationtest/blob/main/README.md), including
* 'cd sourcecode'
* 'git clone https://github.com/philiprodrigues/integrationtest.git'
* 'pip install -r integrationtest/requirements.txt'
* 'cd ..'

Here is a sample command for invoking a test:

* 'pytest -s six_process_stop_start_test.py --frame-file <path>/frames.bin'

For reference, here are the ideas behind the existing tests:
* four_process_disabled_output_test.py - verify that the --disable-data-storage option works
* four_process_empty_run_test.py - verify that runs with no events don't crash
* six_process_multi_file_test.py - test that the file size maximum works
* six_process_stop_start_test.py - verify that we don't get empty fragments at end run
