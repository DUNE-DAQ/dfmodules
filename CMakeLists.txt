cmake_minimum_required(VERSION 3.12)
project(ddpdemo VERSION 0.2.1)

find_package(daq-cmake REQUIRED)
daq_setup_environment()

# 08-Sep-2020, KAB: the inclusion of the appfwk package brings in several other packages,
# include ERS, TRACE, and Boost
find_package(appfwk REQUIRED)
find_package(HighFive REQUIRED)
find_package(dfmessages REQUIRED)

##############################################################################
daq_add_library( StorageKey.cpp 
                 LINK_LIBRARIES 
                 ers::ers HighFive appfwk::appfwk stdc++fs dfmessages::dfmessages )

daq_add_plugin( HDF5DataStore      duneDataStore LINK_LIBRARIES HighFive appfwk::appfwk stdc++fs)
daq_add_plugin( TrashCanDataStore  duneDataStore LINK_LIBRARIES appfwk::appfwk)

daq_add_plugin( DataGenerator      duneDAQModule SCHEMA LINK_LIBRARIES ddpdemo )
daq_add_plugin( DataTransferModule duneDAQModule SCHEMA LINK_LIBRARIES ddpdemo stdc++fs )
daq_add_plugin( GetAllKeysTest     duneDAQModule LINK_LIBRARIES ddpdemo )
daq_add_plugin( SimpleDiskReader   duneDAQModule LINK_LIBRARIES ddpdemo )
daq_add_plugin( SimpleDiskWriter   duneDAQModule LINK_LIBRARIES ddpdemo )

daq_add_plugin( FakeDataProd       duneDAQModule LINK_LIBRARIES ddpdemo )
daq_add_plugin( FakeDataWriter     duneDAQModule LINK_LIBRARIES ddpdemo )
daq_add_plugin( FakeFragRec        duneDAQModule LINK_LIBRARIES ddpdemo )
daq_add_plugin( FakeReqGen         duneDAQModule LINK_LIBRARIES ddpdemo )
daq_add_plugin( FakeTrigDecEmu     duneDAQModule SCHEMA LINK_LIBRARIES ddpdemo )

##############################################################################
daq_add_unit_test( StorageKey_test          LINK_LIBRARIES ddpdemo )
daq_add_unit_test( KeyedDataBlock_test      LINK_LIBRARIES ddpdemo )
daq_add_unit_test( HDF5KeyTranslator_test   LINK_LIBRARIES ddpdemo )
daq_add_unit_test( HDF5FileUtils_test       LINK_LIBRARIES ddpdemo )
daq_add_unit_test( HDF5Write_test           LINK_LIBRARIES ddpdemo )
daq_add_unit_test( HDF5Read_test            LINK_LIBRARIES ddpdemo )
daq_add_unit_test( HDF5GetAllKeys_test      LINK_LIBRARIES ddpdemo )
daq_add_unit_test( HDF5Combiner_test        LINK_LIBRARIES ddpdemo )
daq_add_unit_test( DataStoreFactory_test    LINK_LIBRARIES ddpdemo )

##############################################################################

daq_install()
