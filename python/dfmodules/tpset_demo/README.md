# tpset_demo

This is a demo system that runs TriggerPrimativeMaker` to read TPs
from a file and send them to a TPSetWriter running in another process.

## Generating the system configuration files

The `dfmodules.tpset_demo.toplevel` app generator will create the metadata
for `nanorc` to execute.

```
python -m dfmodules.tpset_demo.toplevel -s 10 -f <file> tpset_demo
```

## Running the demo system

```
nanorc tpset_demo boot init conf start 101 wait 20 stop scrap terminate
```

## Data files

As of 23-Jun, there are a couple of TP files available from cernbox.

To download them, you can use the following commands:

    curl -o tps_link_05.txt -O https://cernbox.cern.ch/index.php/s/75PDf54a9DWXWOJ/download?files=tps_link_05.txt
    curl -o tps_link_11.txt -O https://cernbox.cern.ch/index.php/s/75PDf54a9DWXWOJ/download?files=tps_link_11.txt
