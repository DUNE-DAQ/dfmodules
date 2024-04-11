cmake_minimum_required(VERSION 3.12)
project(dfmodules VERSION 3.0.1)

find_package(daq-cmake REQUIRED)
daq_setup_environment()

find_package(appfwk REQUIRED)
find_package(logging REQUIRED)
find_package(ers REQUIRED)
find_package(hdf5libs REQUIRED)
find_package(dfmessages REQUIRED)
find_package(opmonlib REQUIRED)
find_package(iomanager REQUIRED)
find_package(detdataformats REQUIRED)
find_package(trgdataformats REQUIRED)

# JCF, Dec-1-2021: removed timinglibs
find_package(triggeralgs REQUIRED)
find_package(trigger REQUIRED)
find_package(serialization REQUIRED) ## MR This can probably be removed? Unless we need it for the TPWriter
find_package(readoutlibs REQUIRED)
find_package(Boost COMPONENTS iostreams unit_test_framework REQUIRED)

daq_codegen( *.jsonnet DEP_PKGS hdf5libs TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen( info/*.jsonnet DEP_PKGS opmonlib TEMPLATES opmonlib/InfoStructs.hpp.j2 opmonlib/InfoNljs.hpp.j2 )

##############################################################################
daq_add_library( TriggerInhibitAgent.cpp TriggerRecordBuilderData.cpp TPBundleHandler.cpp
                 LINK_LIBRARIES 
                 opmonlib::opmonlib ers::ers HighFive appfwk::appfwk logging::logging stdc++fs dfmessages::dfmessages utilities::utilities trigger::trigger detdataformats::detdataformats trgdataformats::trgdataformats)

daq_add_plugin( HDF5DataStore      duneDataStore LINK_LIBRARIES logging::logging daqdataformats::daqdataformats hdf5libs::hdf5libs appfwk::appfwk stdc++fs)

daq_add_plugin( FragmentAggregator    duneDAQModule LINK_LIBRARIES dfmodules iomanager::iomanager )
daq_add_plugin( DataWriter            duneDAQModule LINK_LIBRARIES dfmodules hdf5libs::hdf5libs iomanager::iomanager )
daq_add_plugin( DataFlowOrchestrator  duneDAQModule LINK_LIBRARIES dfmodules iomanager::iomanager )
daq_add_plugin( TriggerRecordBuilder  duneDAQModule LINK_LIBRARIES dfmodules iomanager::iomanager )
daq_add_plugin( FakeDataProd          duneDAQModule LINK_LIBRARIES dfmodules iomanager::iomanager)
daq_add_plugin( TPStreamWriter        duneDAQModule LINK_LIBRARIES dfmodules hdf5libs::hdf5libs trigger::trigger serialization::serialization readoutlibs::readoutlibs Boost::iostreams )

##############################################################################
daq_add_unit_test( HDF5FileUtils_test       LINK_LIBRARIES dfmodules )

daq_add_unit_test( HDF5Write_test           LINK_LIBRARIES dfmodules hdf5libs::hdf5libs )
add_dependencies(  HDF5Write_test dfmodules_HDF5DataStore_duneDataStore )

daq_add_unit_test( DataFlowOrchestrator_test LINK_LIBRARIES dfmodules )
add_dependencies( DataFlowOrchestrator_test dfmodules_DataFlowOrchestrator_duneDAQModule)

daq_add_unit_test( TriggerRecordBuilderData_test LINK_LIBRARIES dfmodules )
daq_add_unit_test( DataStoreFactory_test    LINK_LIBRARIES dfmodules )

##############################################################################

daq_install()
