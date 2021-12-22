cmake_minimum_required(VERSION 3.12)
project(dfmodules VERSION 2.4.0)

find_package(daq-cmake REQUIRED)
daq_setup_environment()

find_package(appfwk REQUIRED)
find_package(logging REQUIRED)
find_package(ers REQUIRED)
find_package(HighFive REQUIRED)
find_package(dfmessages REQUIRED)
find_package(opmonlib REQUIRED)
find_package(networkmanager REQUIRED)

# 22-Jul-2021, KAB: all of the packages in this block are needed for the first version
# of the TPSetWriter. Some of these may be able to be removed, at some point in time,
# if we choose different tools for handling the TPSet writing.

# JCF, Dec-1-2021: removed timinglibs
find_package(triggeralgs REQUIRED)
find_package(trigger REQUIRED)
find_package(serialization REQUIRED)
find_package(readoutlibs REQUIRED)
find_package(Boost COMPONENTS iostreams unit_test_framework REQUIRED)

daq_codegen( *.jsonnet TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen( info/*.jsonnet DEP_PKGS opmonlib TEMPLATES opmonlib/InfoStructs.hpp.j2 opmonlib/InfoNljs.hpp.j2 )

##############################################################################
daq_add_library( StorageKey.cpp TriggerDecisionForwarder.cpp TriggerInhibitAgent.cpp TriggerRecordBuilderData.cpp
                 LINK_LIBRARIES 
                 ers::ers HighFive appfwk::appfwk logging::logging stdc++fs dfmessages::dfmessages utilities::utilities)

daq_add_plugin( HDF5DataStore      duneDataStore LINK_LIBRARIES logging::logging daqdataformats::daqdataformats HighFive appfwk::appfwk stdc++fs)

daq_add_plugin( DataWriter            duneDAQModule LINK_LIBRARIES dfmodules networkmanager::networkmanager )
daq_add_plugin( DataFlowOrchestrator  duneDAQModule LINK_LIBRARIES dfmodules networkmanager::networkmanager )
daq_add_plugin( TriggerRecordBuilder  duneDAQModule LINK_LIBRARIES dfmodules networkmanager::networkmanager )
daq_add_plugin( TPSetWriter           duneDAQModule LINK_LIBRARIES dfmodules trigger::trigger serialization::serialization readoutlibs::readoutlibs Boost::iostreams )
daq_add_plugin( FakeDataProd          duneDAQModule LINK_LIBRARIES dfmodules networkmanager::networkmanager)
daq_add_plugin( RequestReceiver       duneDAQModule LINK_LIBRARIES dfmodules networkmanager::networkmanager)
daq_add_plugin( FragmentReceiver       duneDAQModule LINK_LIBRARIES dfmodules networkmanager::networkmanager)
daq_add_plugin( TriggerDecisionReceiver       duneDAQModule LINK_LIBRARIES dfmodules networkmanager::networkmanager)

##############################################################################
daq_add_unit_test( StorageKey_test          LINK_LIBRARIES dfmodules )
daq_add_unit_test( KeyedDataBlock_test      LINK_LIBRARIES dfmodules )

daq_add_unit_test( HDF5KeyTranslator_test   LINK_LIBRARIES dfmodules )
add_dependencies(  HDF5KeyTranslator_test dfmodules_HDF5DataStore_duneDataStore)

daq_add_unit_test( HDF5FileUtils_test       LINK_LIBRARIES dfmodules )

daq_add_unit_test( HDF5Write_test           LINK_LIBRARIES dfmodules )
add_dependencies(  HDF5Write_test dfmodules_HDF5DataStore_duneDataStore)

daq_add_unit_test( HDF5Read_test            LINK_LIBRARIES dfmodules )
add_dependencies(  HDF5Read_test dfmodules_HDF5DataStore_duneDataStore)

daq_add_unit_test( TriggerRecordHeaderWrite_test  LINK_LIBRARIES dfmodules )
add_dependencies(  TriggerRecordHeaderWrite_test dfmodules_HDF5DataStore_duneDataStore)

#daq_add_unit_test( HDF5GetAllKeys_test      LINK_LIBRARIES dfmodules )
daq_add_unit_test( DataStoreFactory_test    LINK_LIBRARIES dfmodules )

##############################################################################

daq_install()
