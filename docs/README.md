### Introduction

The _dfmodules_ repository contains the DAQModules that are used in the Dataflow part of the system, the schema files that are used to specify the configuration of those DAQModules, and helper classes that are used for functionality such as storing data on disk.

The DAQModules in this repository are the following:
* RequestGenerator
   * This module receives TriggerDecision (TD) messages from the DataSelection subsystem (e.g. an instance of the TriggerDecisionEmulator module) and creates DataRequests that are sent to the Readout subsystem based on the components that are requested in the TD message.
* FragmentReceiver
   * This module receives the data fragments from the Readout subsystem and builds them together into complete TriggerRecords (TRs).  
* DataWriter
   * This module stores the TriggerRecords in a configurable format.  Initially, the storage format is HDF5 files on disk, and additional storage options may be added later.   

This repository also currently contains the definition of the DataStore interface and an initial implementation of that interface for HDF5 files on disk (HDF5DataStore).  

### Configuration Parameters

Configuration parameters are used to customize the behavior of these modules, and here are some examples of the parameters that currently exist:
* RequestGenerator
   * the map of requested components to modules in the Readout subsystem that will handle their readout
* FragmentReceiver
   * timeouts for reading from queues and for declaring an incomplete TriggerRecord stale
* DataWriter
   * whether or not to actually store the data or just go through the motions and drop the data on the floor (which is useful sometimes during DAQ system testing)
   * the details of the DataStore implementation to use
* HDF5DataStore
   * the name of the HDF5 file and the directory on disk where it should be written
   * the maximum size of the file

### Error Conditions

Some of the errors that can be encountered by these modules include the following:
* the HDF5DataStore could be mis-configured to use an invalid directory for storing files.  The check for this is at Start (begin-run) time, and when the problem is noticed, an error is reported and the Start of the run should be aborted.
* in exceptional conditions, the FragmentReceiver may not receive all of the necessary Fragments to complete a specific TriggerRecord.  In this case, warning messages are reported when the incomplete TRs become stale, and the incomplete TRs are sent downstream to be stored at Stop (end-run) time.

### Operational Monitoring Metrics

The modules in this package produce operational monitoring metrics to provide visibility into their operation.  Some example quantities that are reported include the following:
* the FragmentReceiver module reports the number of stale TriggerRecords that it has in its buffer.  Ideally, this number will always be zero during a run, but if it starts to grow, that would indicate a problem in creating fragments (upstream) or receiving fragments.
* the DataWriter module reports the number of TRs received and written.  Typically, these two values match, but they may not if data storage has been disabled, or if a data-storage prescale has been specified in the configuration.

### Raw Data Files

The raw data files are written in HDF5 format.  Each TriggerRecord is stored inside a top-level HDF5 Group.  To allow for relatively granular access to the elements of a TriggerRecord, those elements are written into separate HDF5 DataSets.  That is, each Fragment is written into a DataSet, and the TriggerRecordHeader data is written into its own DataSet.  Fragments are grouped by detector type (e.g. TPC), APA, and Link.  Here is a sample of the Groups and DataSets for one event:

```
   GROUP "TriggerRecord00029"
      GROUP "TPC"
         GROUP "APA000"
            DATASET "Link00"
            DATASET "Link01"
      DATASET "TriggerRecordHeader"
```
