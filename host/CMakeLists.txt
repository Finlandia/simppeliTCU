cmake_minimum_required(VERSION 3.14)
project(simppeliTCU_host CXX)

# Builds the firmware as a Linux program (simppeliTCU_host): the web UI is served on
# localhost:8080, MQTT uses the PC's network, and both CAN buses (TWAI and the MCP2515
# second CAN) go to SocketCAN or a log replay.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(FW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../simppeliTCU)
set(SHIM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shims)

# --- PubSubClient (the real library, compiled against the shims) -------------------
set(PUBSUBCLIENT_DIR "" CACHE PATH "Existing PubSubClient checkout (folder containing src/PubSubClient.cpp)")
if(NOT PUBSUBCLIENT_DIR)
    include(FetchContent)
    FetchContent_Declare(
        pubsubclient
        GIT_REPOSITORY https://github.com/knolleary/pubsubclient.git
        GIT_TAG v2.8
    )
    FetchContent_MakeAvailable(pubsubclient)
    set(PUBSUBCLIENT_DIR ${pubsubclient_SOURCE_DIR})
endif()

# --- Firmware version string -------------------------------------------------------
find_package(Git QUIET)
set(GIT_HASH_RAW host)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_SHORT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_SHORT_HASH)
        set(GIT_HASH_RAW "${GIT_SHORT_HASH}-host")
    endif()
endif()

add_executable(simppeliTCU_host
    main.cpp
    sketch.cpp

    # Firmware sources (unchanged)
    ${FW_DIR}/canInterface.cpp
    ${FW_DIR}/canLeafZE1.cpp
    ${FW_DIR}/cliParser.cpp
    ${FW_DIR}/configuration.cpp
    ${FW_DIR}/hassDiscovery.cpp
    ${FW_DIR}/mqttInterface.cpp
    ${FW_DIR}/secondCan.cpp

    # Arduino / ESP32 replacements
    ${SHIM_DIR}/Arduino.cpp
    ${SHIM_DIR}/Preferences.cpp
    ${SHIM_DIR}/WebServer.cpp
    ${SHIM_DIR}/WiFiClient.cpp
    ${SHIM_DIR}/can_host.cpp

    ${PUBSUBCLIENT_DIR}/src/PubSubClient.cpp
)

# Shims must come first so <WiFi.h> etc. resolve to the host versions
target_include_directories(simppeliTCU_host PRIVATE
    ${SHIM_DIR}
    ${FW_DIR}
    ${PUBSUBCLIENT_DIR}/src
)

target_compile_definitions(simppeliTCU_host PRIVATE
    ESP32                # PubSubClient: use std::function callbacks like on the device
    USE_SECOND_CAN=1     # MCP2515 (AV-CAN) is emulated by shims/can_host.cpp
    GIT_HASH_RAW=${GIT_HASH_RAW}
)

# Like the Arduino build, every translation unit sees Arduino.h first
target_compile_options(simppeliTCU_host PRIVATE -include Arduino.h -Wall)

# The sketch is #included by sketch.cpp, so rebuild when it changes
set_property(SOURCE sketch.cpp APPEND PROPERTY OBJECT_DEPENDS ${FW_DIR}/simppeliTCU.ino ${FW_DIR}/ui.h)
