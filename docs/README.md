The _dfmodules_ repository contains the DAQModules that are used in the Dataflow part of the system, the schema files that are used to specify the configuration of those DAQModules, and helper classes that are used for functionality such as storing data on disk.

The DAQModules in this repository are the following:
* RequestGenerator
   * This module receives TriggerDecision (TD) messages from the DataSelection subsystem (e.g. the TriggerDecisionEmulator) and creates DataRequests that are sent to the Readout subsystem based on the components that are requested in the TD message.
* FragmentReceiver
   * This module receives the data fragments from the Readout subsystem and builds them together into complete TriggerRecords (TRs).  
* DataWriter
   * This module stores the TriggerRecords in a configurable format.  Initially, the storage format is HDF5 files on disk, but additional storage options may be added later.   

This repository also currently contains the definition of the DataStore interface and an initial implementation of that interface for HDF5 files on disk (HDF5DataStore).  

_**Configuration parameters**_ are used to customize the behavior of these modules, and here is an overview of some of the existing parameters:
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
   * etc.

