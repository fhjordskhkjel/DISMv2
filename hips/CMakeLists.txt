cmake_minimum_required(VERSION 3.15)
project(HIPS VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Warn for non-Windows builds but allow for development/testing
if(NOT WIN32)
    message(WARNING "HIPS system is primarily designed for Windows. Some features may not work on other platforms.")
    add_definitions(-DCROSS_PLATFORM_BUILD)
endif()

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler-specific options
if(MSVC)
    # Visual Studio specific options
    add_compile_options(/W4 /WX)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 minimum
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    # MinGW/GCC options
    if(WIN32)
        add_compile_options(-Wall -Wextra -pedantic -Werror)
    else()
        # Relaxed warnings for cross-platform development builds
        add_compile_options(-Wall -Wextra)
    endif()
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(HIPS_SOURCES
    src/hips_core.cpp
    src/file_monitor.cpp
    src/process_monitor.cpp
    src/network_monitor.cpp
    src/registry_monitor.cpp
    src/memory_protector.cpp
    src/config_manager.cpp
    src/log_manager.cpp
    src/alert_manager.cpp
)

# Header files
set(HIPS_HEADERS
    include/hips_core.h
    include/file_monitor.h
    include/process_monitor.h
    include/network_monitor.h
    include/registry_monitor.h
    include/memory_protector.h
    include/config_manager.h
    include/log_manager.h
    include/alert_manager.h
)

# Create HIPS library
add_library(hips_lib STATIC ${HIPS_SOURCES} ${HIPS_HEADERS})

# Windows-specific libraries
if(WIN32)
    target_link_libraries(hips_lib
        advapi32
        kernel32
        psapi
        iphlpapi
        ws2_32
        user32
        shell32
    )
else()
    # For cross-platform development builds, link minimal libraries
    target_link_libraries(hips_lib
        pthread
    )
endif()

# Main executable
add_executable(hips src/main.cpp)
target_link_libraries(hips hips_lib)

# Installation
install(TARGETS hips
    DESTINATION bin
)

install(FILES ${HIPS_HEADERS}
    DESTINATION include/hips
)

install(TARGETS hips_lib
    DESTINATION lib
)

# Testing
enable_testing()

# Add test subdirectory if it exists
if(EXISTS ${CMAKE_SOURCE_DIR}/tests)
    add_subdirectory(tests)
endif()

# Package configuration
set(CPACK_PACKAGE_NAME "Advanced HIPS System")
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 0)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Advanced Host Intrusion Prevention System for Windows")
set(CPACK_PACKAGE_VENDOR "HIPS Project")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/../LICENSE")

include(CPack)

# Documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    
    if(EXISTS ${DOXYGEN_IN})
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        
        add_custom_target(doc_doxygen ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
endif()

# Custom targets for development
add_custom_target(format
    COMMAND clang-format -i ${HIPS_SOURCES} ${HIPS_HEADERS} src/main.cpp
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting source code"
)

add_custom_target(analyze
    COMMAND cppcheck --enable=all --std=c++17 ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running static analysis"
)

# Debug information
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")