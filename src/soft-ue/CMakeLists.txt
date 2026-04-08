build_lib(
  LIBNAME soft-ue
  SOURCE_FILES
    helper/soft-ue-helper.cc
    model/common/soft-ue-packet-tag.cc
    model/ses/ses-manager.cc
    model/ses/operation-metadata.cc
    model/ses/msn-entry.cc
    model/pds/pds-manager.cc
    model/pds/pds-header.cc
    model/pds/pds-statistics.cc
    model/pdc/pdc-base.cc
    model/pdc/ipdc.cc
    model/pdc/tpdc.cc
    model/pdc/rto-timer/rto-timer.cc
    model/network/soft-ue-net-device.cc
    model/network/soft-ue-channel.cc
  HEADER_FILES
    helper/soft-ue-helper.h
    model/common/transport-layer.h
    model/common/soft-ue-packet-tag.h
    model/ses/ses-manager.h
    model/ses/operation-metadata.h
    model/ses/msn-entry.h
    model/pds/pds-manager.h
    model/pds/pds-common.h
    model/pdc/pdc-base.h
    model/pdc/ipdc.h
    model/pdc/tpdc.h
    model/pdc/rto-timer/rto-timer.h
    model/network/soft-ue-net-device.h
    model/network/soft-ue-channel.h
  LIBRARIES_TO_LINK ${libnetwork} ${libapplications} ${libinternet} ${libtraffic-control} ${libmobility} ${libcore}
  )

# Add Soft-UE test executables
# Core tests - Test essential Soft-UE functionality
add_executable(ses-manager-test test/ses-manager-test.cc)
target_link_libraries(ses-manager-test ${libsoft-ue})

add_executable(pds-manager-test test/pds-manager-test.cc)
target_link_libraries(pds-manager-test ${libsoft-ue})

add_executable(pds-full-test test/pds-full-test.cc)
target_link_libraries(pds-full-test ${libsoft-ue} ${libnetwork} ${libapplications} ${libinternet} ${libtraffic-control} ${libmobility})

add_executable(rud-semantic-negative-test test/rud-semantic-negative-test.cc)
target_link_libraries(rud-semantic-negative-test ${libsoft-ue} ${libnetwork} ${libapplications} ${libinternet} ${libtraffic-control} ${libmobility})

add_executable(rud-soak-smoke-test test/rud-soak-smoke-test.cc)
target_link_libraries(rud-soak-smoke-test ${libsoft-ue} ${libnetwork} ${libapplications} ${libinternet} ${libtraffic-control} ${libmobility})

add_executable(rud-control-pressure-test test/rud-control-pressure-test.cc)
target_link_libraries(rud-control-pressure-test ${libsoft-ue} ${libnetwork} ${libapplications} ${libinternet} ${libtraffic-control} ${libmobility})

add_executable(rud-jumbo-send-test test/rud-jumbo-send-test.cc)
target_link_libraries(rud-jumbo-send-test ${libsoft-ue} ${libnetwork} ${libapplications} ${libinternet} ${libtraffic-control} ${libmobility})

add_executable(rud-jumbo-write-test test/rud-jumbo-write-test.cc)
target_link_libraries(rud-jumbo-write-test ${libsoft-ue} ${libnetwork} ${libapplications} ${libinternet} ${libtraffic-control} ${libmobility})

add_executable(rud-jumbo-read-test test/rud-jumbo-read-test.cc)
target_link_libraries(rud-jumbo-read-test ${libsoft-ue} ${libnetwork} ${libapplications} ${libinternet} ${libtraffic-control} ${libmobility})

add_executable(soft-ue-bridge-export-test test/soft-ue-bridge-export-test.cc)
target_link_libraries(soft-ue-bridge-export-test ${libsoft-ue} ${libnetwork} ${libapplications} ${libinternet} ${libtraffic-control} ${libmobility})

add_executable(soft-ue-channel-rate-test test/soft-ue-channel-rate-test.cc)
target_link_libraries(soft-ue-channel-rate-test ${libsoft-ue} ${libnetwork} ${libapplications} ${libinternet} ${libtraffic-control} ${libmobility})

# Install test targets
install(TARGETS
  ses-manager-test
  pds-manager-test
  pds-full-test
  rud-semantic-negative-test
  rud-soak-smoke-test
  rud-control-pressure-test
  rud-jumbo-send-test
  rud-jumbo-write-test
  rud-jumbo-read-test
  soft-ue-bridge-export-test
  soft-ue-channel-rate-test
  DESTINATION bin)
