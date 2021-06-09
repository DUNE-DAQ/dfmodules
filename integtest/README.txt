# 09-Jun-2021, KAB: notes on some initial integrationtests...

I followed the instructions from Phil
(https://github.com/philiprodrigues/integrationtest/blob/main/README.md), including
* 'cd sourcecode'
* 'git clone https://github.com/philiprodrigues/integrationtest.git'
* 'pip install -r integrationtest/requirements.txt'
* 'cd ..'

One note is that I needed to specify the nanorc location, since it moved around
a bit in the v2.6.0 release.  So, the command that I would use to invoke the tests
looked like the following:

* 'pytest -s six_process_multi_file_test.py --frame-file <path>/frames.bin --nanorc-path `which nanorc`'

Another note is that I couldn't use the "data_file_checks.sanity_check" on all of
the files in the multi-file test because it seems to assume that the first trigger
number in each file starts with 00001.
