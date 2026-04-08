build_lib(
  LIBNAME sue-sim-module
  SOURCE_FILES
    model/point-to-point-sue-net-device.cc
    model/point-to-point-sue-channel.cc
    model/sue-cbfc-header.cc
    model/sue-header.cc
    model/performance-logger.cc
    model/sue-tag.cc
    model/sue-client.cc
    model/sue-server.cc
    model/traffic-generator.cc
    model/traffic-generator-trace.cc
    model/traffic-generator-config.cc
    model/load-balancer.cc
    model/sue-ppp-header.cc
    model/sue-switch.cc
    model/sue-cbfc.cc
    model/sue-queue-manager.cc
    model/sue-utils.cc
    model/sue-llr.cc
    helper/sue-sim-module-helper.cc
    helper/soft-ue-observer-helper.cc
    model/simulation-config/application-deployer.cc
    model/simulation-config/parameter-config.cc
    model/simulation-config/common-utils.cc
    model/simulation-config/topology-builder.cc
  HEADER_FILES
    model/point-to-point-sue-net-device.h
    model/point-to-point-sue-channel.h
    model/sue-cbfc-header.h
    model/sue-header.h
    model/performance-logger.h
    model/sue-tag.h
    model/sue-client.h
    model/sue-server.h
    model/traffic-generator.h
    model/traffic-generator-trace.h
    model/traffic-generator-config.h
    model/load-balancer.h
    model/sue-ppp-header.h
    model/sue-switch.h
    model/sue-cbfc.h
    model/sue-queue-manager.h
    model/sue-utils.h
    model/sue-llr.h
    helper/sue-sim-module-helper.h
    helper/soft-ue-observer-helper.h
    model/simulation-config/application-deployer.h
    model/simulation-config/parameter-config.h
    model/simulation-config/common-utils.h
    model/simulation-config/topology-builder.h
  LIBRARIES_TO_LINK ${libnetwork} ${libcore} ${libpoint-to-point} ${libsoft-ue}
  TEST_SOURCES test/sue-sim-module-test.cc
)

add_executable(soft-ue-observer-test test/soft-ue-observer-test.cc)
target_link_libraries(soft-ue-observer-test ${libsue-sim-module} ${libsoft-ue} ${libnetwork} ${libcore} ${libpoint-to-point})

add_executable(soft-ue-truth-system-test test/soft-ue-truth-system-test.cc)
target_link_libraries(soft-ue-truth-system-test ${libsue-sim-module} ${libsoft-ue} ${libnetwork} ${libcore} ${libpoint-to-point})

add_executable(soft-ue-truth-pressure-system-test test/soft-ue-truth-pressure-system-test.cc)
target_link_libraries(soft-ue-truth-pressure-system-test ${libsue-sim-module} ${libsoft-ue} ${libnetwork} ${libcore} ${libpoint-to-point})

add_executable(soft-ue-truth-benchmark-smoke-test test/soft-ue-truth-benchmark-smoke-test.cc)
target_link_libraries(soft-ue-truth-benchmark-smoke-test ${libcore})

install(TARGETS
  soft-ue-observer-test
  soft-ue-truth-system-test
  soft-ue-truth-pressure-system-test
  soft-ue-truth-benchmark-smoke-test
  DESTINATION bin)
