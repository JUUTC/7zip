# CMake build system for 7-Zip Parallel Compression Library
# This provides unified cross-platform builds for Linux, Windows, and macOS

cmake_minimum_required(VERSION 3.15)
project(7zip-parallel 
    VERSION 1.0.0 
    DESCRIPTION "7-Zip Parallel Compression Library"
    LANGUAGES C CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build example programs" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ENABLE_AZURE_SUPPORT "Enable Azure Blob Storage support" OFF)
option(ENABLE_AWS_SUPPORT "Enable AWS S3 support" OFF)

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS ON)
    message(STATUS "Building for Windows")
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_LINUX ON)
    message(STATUS "Building for Linux")
elseif(APPLE)
    set(PLATFORM_MACOS ON)
    message(STATUS "Building for macOS")
endif()

# Compiler flags
if(MSVC)
    # Microsoft Visual C++
    add_compile_options(/W3 /WX- /MP)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # GCC or Clang
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
    add_compile_options(-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE)
    
    if(CMAKE_BUILD_TYPE MATCHES Release)
        add_compile_options(-O2 -DNDEBUG)
    else()
        add_compile_options(-g -O0)
    endif()
endif()

# Threading support
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# Include directories
set(COMMON_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/CPP
    ${CMAKE_CURRENT_SOURCE_DIR}/C
)

# ============================================================================
# C Library Components
# ============================================================================

set(C_SOURCES
    C/Alloc.c
    C/LzFind.c
    C/LzmaEnc.c
    C/Lzma2Enc.c
    C/Threads.c
    C/7zCrc.c
    C/7zCrcOpt.c
)

# ============================================================================
# Common C++ Components
# ============================================================================

set(COMMON_SOURCES
    CPP/Common/MyString.cpp
    CPP/Common/IntToString.cpp
    CPP/Common/StringConvert.cpp
    CPP/Common/MyVector.cpp
    CPP/Common/MyWindows.cpp
)

# ============================================================================
# Windows Abstraction Layer
# ============================================================================

set(WINDOWS_SOURCES
    CPP/Windows/FileDir.cpp
    CPP/Windows/FileFind.cpp
    CPP/Windows/FileIO.cpp
    CPP/Windows/FileName.cpp
    CPP/Windows/PropVariant.cpp
    CPP/Windows/Synchronization.cpp
    CPP/Windows/System.cpp
    CPP/Windows/TimeUtils.cpp
    # Note: Thread.h is header-only, wraps C/Threads.c
)

# ============================================================================
# 7zip Common Components
# ============================================================================

set(COMPRESS_COMMON_SOURCES
    CPP/7zip/Common/InBuffer.cpp
    CPP/7zip/Common/OutBuffer.cpp
    CPP/7zip/Common/StreamUtils.cpp
    CPP/7zip/Common/FileStreams.cpp
    CPP/7zip/Common/StreamObjects.cpp
    CPP/7zip/Common/LimitedStreams.cpp
    CPP/7zip/Common/MethodProps.cpp
    CPP/7zip/Common/CreateCoder.cpp
    CPP/7zip/Common/FilterCoder.cpp
    # Note: RegisterCodec.h and RegisterCodec.cpp don't exist as separate files
)

# ============================================================================
# Compression Encoders
# ============================================================================

set(ENCODER_SOURCES
    CPP/7zip/Compress/LzmaEncoder.cpp
    CPP/7zip/Compress/Lzma2Encoder.cpp
)

# ============================================================================
# Parallel Compression Library
# ============================================================================

set(PARALLEL_COMPRESS_SOURCES
    CPP/7zip/Compress/ParallelCompressor.cpp
    CPP/7zip/Compress/ParallelCompressAPI.cpp
    CPP/7zip/Compress/ParallelCompressorRegister.cpp
)

# Combine all sources for the main library
set(LIBRARY_SOURCES
    ${C_SOURCES}
    ${COMMON_SOURCES}
    ${WINDOWS_SOURCES}
    ${COMPRESS_COMMON_SOURCES}
    ${ENCODER_SOURCES}
    ${PARALLEL_COMPRESS_SOURCES}
)

# Create the parallel compression library
add_library(7zip-parallel ${LIBRARY_SOURCES})

target_include_directories(7zip-parallel
    PUBLIC
        ${COMMON_INCLUDES}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/CPP/7zip
)

target_link_libraries(7zip-parallel
    PUBLIC
        Threads::Threads
)

# Platform-specific settings
if(PLATFORM_WINDOWS)
    # Windows-specific settings
    target_compile_definitions(7zip-parallel PRIVATE 
        _WINDOWS
        WIN32
        _WIN32
    )
elseif(PLATFORM_LINUX OR PLATFORM_MACOS)
    # Unix-like systems
    target_compile_definitions(7zip-parallel PRIVATE
        _7ZIP_ST  # Single-threaded COM (no Windows COM)
    )
    
    # Link with pthread and rt if available
    if(PLATFORM_LINUX)
        target_link_libraries(7zip-parallel PUBLIC rt)
    endif()
endif()

# Set library properties
set_target_properties(7zip-parallel PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "CPP/7zip/Compress/ParallelCompressAPI.h;CPP/7zip/Compress/ParallelCompressor.h"
)

# ============================================================================
# Installation
# ============================================================================

install(TARGETS 7zip-parallel
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/7zip
)

# Install additional headers
install(DIRECTORY CPP/7zip/
    DESTINATION include/7zip
    FILES_MATCHING PATTERN "*.h"
)

# ============================================================================
# Examples
# ============================================================================

if(BUILD_EXAMPLES)
    message(STATUS "Building examples")
    
    # Simple example
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/CPP/7zip/Compress/SimpleTest.cpp)
        add_executable(SimpleTest CPP/7zip/Compress/SimpleTest.cpp)
        target_link_libraries(SimpleTest 7zip-parallel)
        target_include_directories(SimpleTest PRIVATE ${COMMON_INCLUDES})
    endif()
    
    # Parallel compress example
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/CPP/7zip/Compress/ParallelCompressExample.cpp)
        add_executable(ParallelCompressExample CPP/7zip/Compress/ParallelCompressExample.cpp)
        target_link_libraries(ParallelCompressExample 7zip-parallel)
        target_include_directories(ParallelCompressExample PRIVATE ${COMMON_INCLUDES})
    endif()
endif()

# ============================================================================
# Tests
# ============================================================================

if(BUILD_TESTS)
    enable_testing()
    message(STATUS "Building test suite")
    
    # Helper function to add a test executable
    function(add_7zip_test TEST_NAME TEST_SOURCE)
        add_executable(${TEST_NAME} ${TEST_SOURCE})
        target_link_libraries(${TEST_NAME} 7zip-parallel)
        target_include_directories(${TEST_NAME} PRIVATE ${COMMON_INCLUDES})
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endfunction()
    
    # Add test executables
    set(TEST_DIR CPP/7zip/Compress)
    
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/ParallelCompressorTest.cpp)
        add_7zip_test(ParallelCompressorTest ${TEST_DIR}/ParallelCompressorTest.cpp)
    endif()
    
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/ParallelCompressorValidation.cpp)
        add_7zip_test(ParallelCompressorValidation ${TEST_DIR}/ParallelCompressorValidation.cpp)
    endif()
    
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/ParallelE2ETest.cpp)
        add_7zip_test(ParallelE2ETest ${TEST_DIR}/ParallelE2ETest.cpp)
    endif()
    
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/ParallelFeatureParityTest.cpp)
        add_7zip_test(ParallelFeatureParityTest ${TEST_DIR}/ParallelFeatureParityTest.cpp)
    endif()
    
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/ParallelIntegrationTest.cpp)
        add_7zip_test(ParallelIntegrationTest ${TEST_DIR}/ParallelIntegrationTest.cpp)
    endif()
    
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/ParallelSolidMultiVolumeTest.cpp)
        add_7zip_test(ParallelSolidMultiVolumeTest ${TEST_DIR}/ParallelSolidMultiVolumeTest.cpp)
    endif()
    
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/ParallelSecurityTest.cpp)
        add_7zip_test(ParallelSecurityTest ${TEST_DIR}/ParallelSecurityTest.cpp)
    endif()
endif()

# ============================================================================
# Cloud Storage Support (Optional)
# ============================================================================

if(ENABLE_AZURE_SUPPORT)
    message(STATUS "Azure Blob Storage support enabled")
    # Find Azure SDK
    # find_package(azure-storage-blobs-cpp CONFIG REQUIRED)
    # target_link_libraries(7zip-parallel PUBLIC Azure::azure-storage-blobs)
    target_compile_definitions(7zip-parallel PUBLIC ENABLE_AZURE_BLOB_SUPPORT)
endif()

if(ENABLE_AWS_SUPPORT)
    message(STATUS "AWS S3 support enabled")
    # Find AWS SDK
    # find_package(AWSSDK REQUIRED COMPONENTS s3)
    # target_link_libraries(7zip-parallel PUBLIC ${AWSSDK_LINK_LIBRARIES})
    target_compile_definitions(7zip-parallel PUBLIC ENABLE_AWS_S3_SUPPORT)
endif()

# ============================================================================
# Package Configuration
# ============================================================================

include(CMakePackageConfigHelpers)

# Generate package config files
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/7zip-parallelConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/7zip-parallelConfig.cmake"
    INSTALL_DESTINATION lib/cmake/7zip-parallel
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/7zip-parallelConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/7zip-parallelConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/7zip-parallelConfigVersion.cmake"
    DESTINATION lib/cmake/7zip-parallel
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "7-Zip Parallel Compression Library Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "Build Options:")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Build examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  Azure support: ${ENABLE_AZURE_SUPPORT}")
message(STATUS "  AWS support: ${ENABLE_AWS_SUPPORT}")
message(STATUS "")
