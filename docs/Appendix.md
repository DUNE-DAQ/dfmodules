DOCUMENTATION APPENDIX FOR THE ELJELINK_CHANGES BRANCH

A new DAQ module TrSender was added to the dfmodules package. 

The TrSender module creates trigger records according to the specified configuration and sends them to the DataWriter module. Subsequently, Datawriter writes them to a hdf5 file. The hdf5 file parameters are also specified by the configuration. 

-------------------------------------------------------------------------------------------------------
In order to run this process you should create a configuration with command:

sender_gen -c [json file path] [name of the configuration folder]

An example json configuration file is placed in scripts folder. 
After creating configuration, we are able to run it with nanorc: 

nanorc [name of the configuration folder] [partition name]

and standard commands for nanorc. 
-------------------------------------------------------------------------------------------------------
In log file, the complete info about the whole process is visible, including the rate od writing and number of received tokens etc..
