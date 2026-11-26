cmake_minimum_required(VERSION 3.14)
project(simppeliTCU_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest QUIET)
if(GTest_FOUND)
    set(GTEST_MAIN_TARGET GTest::gtest_main)
else()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
    set(GTEST_MAIN_TARGET gtest_main)
endif()

add_library(test_stubs STATIC
    test_canInterface_stubs.cpp
)

target_include_directories(test_stubs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(can_leaf_ze1_parsing_tests
    test_canLeafZE1_parsing.cpp
    ../simppeliTCU/canLeafZE1.cpp
    ../simppeliTCU/vehicleTypes.h
    ../simppeliTCU/canInterface.h
    ../simppeliTCU/canLeafZE1.h
)

target_include_directories(can_leaf_ze1_parsing_tests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../simppeliTCU
)

# Enables test-only hooks in the firmware sources (e.g. resetCanLeafZE1State)
target_compile_definitions(can_leaf_ze1_parsing_tests PRIVATE SIMPPELITCU_UNIT_TEST)

target_link_libraries(can_leaf_ze1_parsing_tests PRIVATE
    ${GTEST_MAIN_TARGET}
    test_stubs
)

enable_testing()

# Shuffle test order on every run to catch state leaking between tests.
# A failing run prints its seed; reproduce with --gtest_shuffle --gtest_random_seed=<seed>.
add_test(NAME can_leaf_ze1_parsing_tests COMMAND can_leaf_ze1_parsing_tests --gtest_shuffle)

# --- canInterface.cpp ------------------------------------------------------------
# The real canInterface.cpp is compiled against fakes in fakes/ (Arduino, TWAI driver,
# MCP2515 second CAN, handleReceivedMessage). fakes/ comes before host/shims so that
# <Arduino.h> resolves to the test fake; host/shims only supplies driver/twai.h (types).
# Built twice so both USE_SECOND_CAN variants are tested.
function(add_can_interface_test name use_second_can)
    add_executable(${name}
        test_canInterface.cpp
        fakes/fake_arduino.cpp
        fakes/fake_can_drivers.cpp
        ../simppeliTCU/canInterface.cpp
    )
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/fakes
        ${CMAKE_CURRENT_SOURCE_DIR}/../simppeliTCU
        ${CMAKE_CURRENT_SOURCE_DIR}/../host/shims
    )
    target_compile_definitions(${name} PRIVATE
        SIMPPELITCU_UNIT_TEST
        USE_SECOND_CAN=${use_second_can}
    )
    target_link_libraries(${name} PRIVATE ${GTEST_MAIN_TARGET})
    add_test(NAME ${name} COMMAND ${name} --gtest_shuffle)
endfunction()

add_can_interface_test(can_interface_tests 1)
add_can_interface_test(can_interface_no_second_can_tests 0)

# --- Remaining firmware sources (built against the host shims) --------------------
# These sources use the Arduino/ESP32 API heavily (String, Preferences, WiFi, PubSubClient,
# WebServer), so they are compiled against the same shims as the host application
# (host/shims) plus the real PubSubClient library, with the same flags as host/CMakeLists.txt.
# The host shims use POSIX APIs (sockets, poll), so these suites build on Linux/WSL/macOS only.
if(UNIX)
set(FW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../simppeliTCU)
set(HOST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../host)
set(SHIM_DIR ${HOST_DIR}/shims)

include(FetchContent)
FetchContent_Declare(
    pubsubclient
    GIT_REPOSITORY https://github.com/knolleary/pubsubclient.git
    GIT_TAG v2.8
)
FetchContent_MakeAvailable(pubsubclient)

add_library(host_shims STATIC
    ${SHIM_DIR}/Arduino.cpp
    ${SHIM_DIR}/Preferences.cpp
    ${SHIM_DIR}/WebServer.cpp
    ${SHIM_DIR}/WiFiClient.cpp
    ${SHIM_DIR}/can_host.cpp
    ${pubsubclient_SOURCE_DIR}/src/PubSubClient.cpp
)
# Shims come first so <WiFi.h> etc. resolve to the host versions
target_include_directories(host_shims PUBLIC
    ${SHIM_DIR}
    ${FW_DIR}
    ${pubsubclient_SOURCE_DIR}/src
)
target_compile_definitions(host_shims PUBLIC
    ESP32                # PubSubClient: std::function callbacks like on the device
    SIMPPELITCU_UNIT_TEST
)
# Like the Arduino build, every translation unit sees Arduino.h first
target_compile_options(host_shims PUBLIC -include Arduino.h)

# add_host_test(<name> SOURCES <files...> [DEFINITIONS <defs...>] [INCLUDES <dirs...>])
function(add_host_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEFINITIONS;INCLUDES" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${ARG_INCLUDES})
    if(ARG_DEFINITIONS)
        target_compile_definitions(${name} PRIVATE ${ARG_DEFINITIONS})
    else()
        target_compile_definitions(${name} PRIVATE USE_SECOND_CAN=0)
    endif()
    target_link_libraries(${name} PRIVATE ${GTEST_MAIN_TARGET} host_shims)
    add_test(NAME ${name} COMMAND ${name} --gtest_shuffle)
endfunction()

add_host_test(string_buffer_tests SOURCES test_stringBuffer.cpp)

add_host_test(configuration_tests SOURCES
    test_configuration.cpp
    ${FW_DIR}/configuration.cpp
)

add_host_test(cli_parser_tests SOURCES
    test_cliParser.cpp
    ${FW_DIR}/cliParser.cpp
    ${FW_DIR}/configuration.cpp
)

add_host_test(hass_discovery_tests SOURCES
    test_hassDiscovery.cpp
    ${FW_DIR}/hassDiscovery.cpp
    ${FW_DIR}/configuration.cpp
)

add_host_test(mqtt_interface_tests SOURCES
    test_mqttInterface.cpp
    ${FW_DIR}/mqttInterface.cpp
    ${FW_DIR}/hassDiscovery.cpp
    ${FW_DIR}/configuration.cpp
)

# MCP2515 library and SPI are faked; the rest of the Arduino API comes from the host shims
add_host_test(second_can_tests
    SOURCES
        test_secondCan.cpp
        fakes/second_can/fake_mcp2515.cpp
        ${FW_DIR}/secondCan.cpp
    DEFINITIONS USE_SECOND_CAN=1
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/fakes/second_can
)

# ui.h defines sendMainPage() in the header, so it gets its own executable
add_host_test(ui_tests SOURCES test_ui.cpp)
set_property(SOURCE test_ui.cpp APPEND PROPERTY OBJECT_DEPENDS ${FW_DIR}/ui.h)

# The sketch is compiled through host/sketch.cpp (adds the prototypes the Arduino IDE
# would generate) and linked with all other firmware sources, like the host application.
add_host_test(sketch_tests SOURCES
    test_sketch.cpp
    ${HOST_DIR}/sketch.cpp
    ${FW_DIR}/canInterface.cpp
    ${FW_DIR}/canLeafZE1.cpp
    ${FW_DIR}/cliParser.cpp
    ${FW_DIR}/configuration.cpp
    ${FW_DIR}/hassDiscovery.cpp
    ${FW_DIR}/mqttInterface.cpp
)
set_property(SOURCE ${HOST_DIR}/sketch.cpp APPEND PROPERTY OBJECT_DEPENDS ${FW_DIR}/simppeliTCU.ino ${FW_DIR}/ui.h)
else()
    message(STATUS "Not a UNIX host: skipping the host-shim based test suites")
endif()

# --- Coverage report -------------------------------------------------------------
# Configure with -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage -O0", run ctest,
# then `cmake --build . --target coverage`. Produces:
#   coverage/index.html  per-source-file HTML with line and branch highlighting
#   coverage.xml         Cobertura report (Codecov)
# Only firmware sources (simppeliTCU/) are reported; tests, fakes, host shims and
# GoogleTest are left out. The filter is absolute: gcovr anchors relative filters to
# the working directory, not to --root.
find_program(GCOVR_EXECUTABLE gcovr)
if(GCOVR_EXECUTABLE)
    get_filename_component(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)
    set(COVERAGE_DIR ${CMAKE_CURRENT_BINARY_DIR}/coverage)
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${COVERAGE_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
        COMMAND ${GCOVR_EXECUTABLE}
            --root ${REPO_ROOT}
            --object-directory ${CMAKE_CURRENT_BINARY_DIR}
            --filter ${REPO_ROOT}/simppeliTCU/
            --html-details ${COVERAGE_DIR}/index.html
            --html-title "simppeliTCU unit-test coverage"
            --xml ${CMAKE_CURRENT_BINARY_DIR}/coverage.xml
            --print-summary
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating coverage report in ${COVERAGE_DIR}/index.html"
        VERBATIM
    )
else()
    message(STATUS "gcovr not found: 'coverage' target not available")
endif()
