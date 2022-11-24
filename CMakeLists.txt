cmake_minimum_required(VERSION 3.12)
project(dfmodules VERSION 2.9.2)

find_package(daq-cmake REQUIRED)
daq_setup_environment()

find_package(appfwk REQUIRED)
find_package(logging REQUIRED)
find_package(ers REQUIRED)
find_package(daqconf REQUIRED)
find_package(hdf5libs REQUIRED)
find_package(dfmessages REQUIRED)
find_package(opmonlib REQUIRED)
find_package(iomanager REQUIRED)
find_package(detdataformats REQUIRED)

find_package(HighFive REQUIRED)
find_package(daqdataformats REQUIRED)
find_package(detchannelmaps REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(cetlib REQUIRED)

# JCF, Dec-1-2021: removed timinglibs
find_package(triggeralgs REQUIRED)
find_package(trigger REQUIRED)
find_package(serialization REQUIRED) ## MR This can probably be removed? Unless we need it for the TPWriter
find_package(readoutlibs REQUIRED)
find_package(Boost COMPONENTS iostreams unit_test_framework REQUIRED)

daq_codegen( datafloworchestrator.jsonnet DEP_PKGS hdf5libs TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen( datawriter.jsonnet DEP_PKGS hdf5libs TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen( fakedataprod.jsonnet DEP_PKGS hdf5libs TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen( fragmentreceiver.jsonnet DEP_PKGS hdf5libs TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen( hdf5datastore.jsonnet DEP_PKGS hdf5libs TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen( requestreceiver.jsonnet DEP_PKGS hdf5libs TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen( tpstreamwriter.jsonnet DEP_PKGS hdf5libs TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen( triggerdecisionreceiver.jsonnet DEP_PKGS hdf5libs TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen( triggerrecordbuilder.jsonnet DEP_PKGS hdf5libs TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen( info/*.jsonnet DEP_PKGS opmonlib TEMPLATES opmonlib/InfoStructs.hpp.j2 opmonlib/InfoNljs.hpp.j2 )
daq_codegen( confgen.jsonnet DEP_PKGS daqconf TEMPLATES Structs.hpp.j2)
daq_codegen( trsender.jsonnet TEMPLATES Structs.hpp.j2 Nljs.hpp.j2)
#daq_codegen( trsenderinfo.jsonnet DEP_PKGS opmonlib TEMPLATES opmonlib/InfoStructs.hpp.j2 opmonlib/InfoNljs.hpp.j2)
#daq_codegen( receiverinfo.jsonnet DEP_PKGS opmonlib TEMPLATES opmonlib/InfoStructs.hpp.j2 opmonlib/InfoNljs.hpp.j2)

##############################################################################
daq_add_library( TriggerDecisionForwarder.cpp TriggerInhibitAgent.cpp TriggerRecordBuilderData.cpp TPBundleHandler.cpp
                 LINK_LIBRARIES 
                 opmonlib::opmonlib ers::ers HighFive hdf5libs::hdf5libs appfwk::appfwk logging::logging stdc++fs dfmessages::dfmessages daqdataformats::daqdataformats utilities::utilities trigger::trigger detdataformats::detdataformats detchannelmaps::detchannelmaps logging::logging nlohmann_json::nlohmann_json ${CETLIB} ${CETLIB_EXCEPT})

daq_add_plugin( HDF5DataStore      duneDataStore LINK_LIBRARIES logging::logging daqdataformats::daqdataformats hdf5libs::hdf5libs appfwk::appfwk stdc++fs)

daq_add_plugin( DataWriter            duneDAQModule LINK_LIBRARIES dfmodules iomanager::iomanager )
daq_add_plugin( DataFlowOrchestrator  duneDAQModule LINK_LIBRARIES dfmodules iomanager::iomanager )
daq_add_plugin( TriggerRecordBuilder  duneDAQModule LINK_LIBRARIES dfmodules iomanager::iomanager )
daq_add_plugin( FakeDataProd          duneDAQModule LINK_LIBRARIES dfmodules iomanager::iomanager)
daq_add_plugin( RequestReceiver       duneDAQModule LINK_LIBRARIES dfmodules iomanager::iomanager)
daq_add_plugin( FragmentReceiver      duneDAQModule LINK_LIBRARIES dfmodules iomanager::iomanager)
daq_add_plugin( TriggerDecisionReceiver duneDAQModule LINK_LIBRARIES dfmodules iomanager::iomanager)
daq_add_plugin( TPStreamWriter        duneDAQModule LINK_LIBRARIES dfmodules hdf5libs::hdf5libs trigger::trigger serialization::serialization readoutlibs::readoutlibs Boost::iostreams )
daq_add_plugin( TrSender duneDAQModule LINK_LIBRARIES dfmodules iomanager::iomanager) 
daq_add_plugin( Receiver duneDAQModule LINK_LIBRARIES dfmodules iomanager::iomanager) 

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
