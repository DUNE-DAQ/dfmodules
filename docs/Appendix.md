DOCUMENTATION APPENDIX FOR THE ELJELINK_CHANGES BRANCH

A new DAQ module TrSender was added to the dfmodules package. 
The TrSender module creates trigger records according to the specified configuration and sends them to the DataWriter module. Subsequently, Datawriter writes them to a .hdf5 file. The .hdf5 file parameters are also specified by the configuration
-------------------------------------------------------------------------------------------------------
