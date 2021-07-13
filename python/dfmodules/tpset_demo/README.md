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
