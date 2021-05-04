cmake_minimum_required(VERSION 3.12)
project(dfmodules VERSION 2.0.2)

find_package(daq-cmake REQUIRED)
daq_setup_environment()

# 08-Sep-2020, KAB: the inclusion of the appfwk package brings in several other packages,
# include ERS, TRACE, and Boost
find_package(appfwk REQUIRED)
find_package(logging REQUIRED)
find_package(HighFive REQUIRED)
find_package(dfmessages REQUIRED)

daq_codegen(*.jsonnet DEP_PKGS dfmessages TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )

##############################################################################
daq_add_library( StorageKey.cpp TriggerDecisionForwarder.cpp TriggerInhibitAgent.cpp
                 LINK_LIBRARIES 
                 ers::ers HighFive appfwk::appfwk logging::logging stdc++fs dfmessages::dfmessages )

daq_add_plugin( HDF5DataStore      duneDataStore LINK_LIBRARIES logging::logging dataformats::dataformats HighFive appfwk::appfwk stdc++fs)

daq_add_plugin( DataGenerator         duneDAQModule LINK_LIBRARIES dfmodules )
daq_add_plugin( DataTransferModule    duneDAQModule LINK_LIBRARIES dfmodules stdc++fs )
daq_add_plugin( DataWriter            duneDAQModule LINK_LIBRARIES dfmodules )
daq_add_plugin( FakeDataProd          duneDAQModule LINK_LIBRARIES dfmodules )
daq_add_plugin( FragmentReceiver      duneDAQModule LINK_LIBRARIES dfmodules )
daq_add_plugin( FakeReqGen            duneDAQModule LINK_LIBRARIES dfmodules )
daq_add_plugin( RequestGenerator      duneDAQModule LINK_LIBRARIES dfmodules )
daq_add_plugin( FakeTrigDecEmu        duneDAQModule LINK_LIBRARIES dfmodules )
daq_add_plugin( TriggerRecordBuilder  duneDAQModule LINK_LIBRARIES dfmodules )



##############################################################################
#daq_add_unit_test( StorageKey_test          LINK_LIBRARIES dfmodules )
#daq_add_unit_test( KeyedDataBlock_test      LINK_LIBRARIES dfmodules )

daq_add_unit_test( HDF5KeyTranslator_test   LINK_LIBRARIES dfmodules )
add_dependencies(  HDF5KeyTranslator_test dfmodules_HDF5DataStore_duneDataStore)

#daq_add_unit_test( HDF5FileUtils_test       LINK_LIBRARIES dfmodules )

daq_add_unit_test( HDF5Write_test           LINK_LIBRARIES dfmodules )
add_dependencies(  HDF5Write_test dfmodules_HDF5DataStore_duneDataStore)

daq_add_unit_test( HDF5Read_test            LINK_LIBRARIES dfmodules )
add_dependencies(  HDF5Read_test dfmodules_HDF5DataStore_duneDataStore)

daq_add_unit_test( TriggerRecordHeaderWrite_test  LINK_LIBRARIES dfmodules )
add_dependencies(  TriggerRecordHeaderWrite_test dfmodules_HDF5DataStore_duneDataStore)

#daq_add_unit_test( HDF5GetAllKeys_test      LINK_LIBRARIES dfmodules )
#daq_add_unit_test( HDF5Combiner_test        LINK_LIBRARIES dfmodules )
#daq_add_unit_test( DataStoreFactory_test    LINK_LIBRARIES dfmodules )

##############################################################################

daq_install()
