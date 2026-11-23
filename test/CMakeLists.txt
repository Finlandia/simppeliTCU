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
