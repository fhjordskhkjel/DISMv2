cmake_minimum_required(VERSION 3.15)

# Find Google Test
find_package(GTest REQUIRED)
include(GoogleTest)

# Test executable for HIPS core functionality
add_executable(test_hips_core
    test_hips_core.cpp
)

target_link_libraries(test_hips_core
    hips_lib
    GTest::gtest
    GTest::gtest_main
)

# Test executable for file monitor
add_executable(test_file_monitor
    test_file_monitor.cpp
)

target_link_libraries(test_file_monitor
    hips_lib
    GTest::gtest
    GTest::gtest_main
)

# Test executable for process monitor
add_executable(test_process_monitor
    test_process_monitor.cpp
)

target_link_libraries(test_process_monitor
    hips_lib
    GTest::gtest
    GTest::gtest_main
)

# Integration tests
add_executable(test_integration
    test_integration.cpp
)

target_link_libraries(test_integration
    hips_lib
    GTest::gtest
    GTest::gtest_main
)

# Add all tests to CTest
gtest_discover_tests(test_hips_core)
gtest_discover_tests(test_file_monitor)
gtest_discover_tests(test_process_monitor)
gtest_discover_tests(test_integration)

# Custom test target to run all tests
add_custom_target(run_tests
    COMMAND test_hips_core
    COMMAND test_file_monitor
    COMMAND test_process_monitor
    COMMAND test_integration
    DEPENDS test_hips_core test_file_monitor test_process_monitor test_integration
    COMMENT "Running all HIPS tests"
)