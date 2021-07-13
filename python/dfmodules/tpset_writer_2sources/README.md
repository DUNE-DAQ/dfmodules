# tpset_writer_2sources

This is a demo system that runs TriggerPrimativeMaker` to read TPs
from a file and send them to a TPSetWriter running in another process.

## Generating the system configuration files

The `dfmodules.tpset_writer_2sources.toplevel` app generator will create the metadata
for `nanorc` to execute.

```
python -m dfmodules.tpset_writer_2sources.toplevel -s 10 -f <file> tpset_writer_2sources
```

## Running the demo system

```
nanorc tpset_writer_2sources boot init conf start 101 wait 20 stop scrap terminate
```
