cmake_minimum_required(VERSION 3.20)
project(vespera_tests CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_BUILD_TYPE Debug)

# =============================================================================
# Shared stub library
# =============================================================================
# Both test binaries link against the same kernel stubs.

add_library(kernel_stubs STATIC
        stub_kernel/memory.cpp
        stub_kernel/log.cpp
        stub_kernel/time.cpp
        stub_kernel/systemmanager.cpp
)
target_include_directories(kernel_stubs PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/../include
)

# =============================================================================
# CTest
# =============================================================================

enable_testing()

# =============================================================================
# Test Modules
# =============================================================================

add_subdirectory(fat32)
add_subdirectory(ext4)
add_subdirectory(heap)
add_subdirectory(vector)
