21-Mar-2021, KAB: Some notes on Dataflow topics for v2.6.0

Questions to ask:
1. Will we need to read fragment-like data out of the Trigger App to store it in the TriggerRecords?  Better said, will we need to store the Timing TPs that happened around the time of the trigger in the TriggerRecord?
1. Is it OK to stick with a single DataWriter for v2.6.0.  (I presume Yes.)
1. How many different types of DataSelection modules should we attempt to include?  Clearly, we would want something that has equivalent functionality to the existing TriggerDecisionEmulator, and that is probably a simple Module-Level Trigger.  We may not *need* much more than that, but we may *want* more than that, as a proof of concept and to get a head-start on features that will be needed later.
1. How will we geographically identify DAPHNE readout units?  APA# and Link#, or something different?  If APA# and Link#, will we want a detector-type field in GeoID?

Things that I suspect that we will need to take care of and should probably start talking about soon:
1. Work together with Software Coordination and Offline folks to make the _dataformats_ package available to both online and offline.
1. When running with the TDE in a separate process from the Readout and Dataflow modules, there are still warning messages from the 'trig' process about being unable to push TimeSync messages onto the appropriate queue.  I most recently saw this when I stopped the 'trig' process before the RUDF process.  This needs to be worked out.
1. We probably also need to work out how to handle TimeSync resets (or non-resets) and the 2nd, 3rd, etc. run start within a given session of the DAQ.
1. Provide a sample HDF5 file from a MiniDAQ test to Offline folks.  It would be nice to use as much real DAQ and CE electronics in this test as possible.  That will provide some motivation for us to describe the lower-level data format details, and provide an opportunity for offline folks to ask questions about that.

To-do:
1. Provide serialization for DataRequests and Fragments.
1. Discuss whether we can/should provide system configuration information in raw-data-file-metadata information that we provide to offline.

Some possible forward-looking ideas (to be discussed before we start any work on these):
1. Investigate long-window readout as a hook to start thinking about storing data in TimeSlices instead of TriggerRecords?
1. Investigate distributed event building?

Cleanup tasks
1. Make use of generated configuration objects in unit tests.
