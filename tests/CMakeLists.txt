cmake_minimum_required(VERSION 3.20)
project(vespera_tests CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_BUILD_TYPE Debug)

get_filename_component(PROJECT_ROOT ${CMAKE_SOURCE_DIR}/.. ABSOLUTE)

include_directories(
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/fat32
        ${CMAKE_SOURCE_DIR}/../
        ${PROJECT_ROOT}/include
)

# =============================================================================
# Shared stub library
# =============================================================================
# Both test binaries link against the same kernel stubs.

add_library(kernel_stubs STATIC
        stub_kernel/memory.cpp
        stub_kernel/log.cpp
        stub_kernel/time.cpp
)
target_include_directories(kernel_stubs PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/../
        ${PROJECT_ROOT}/include
)

# =============================================================================
# FAT32 tests
# =============================================================================

add_executable(fat32_tests
        ../filesystem/fat32/fat32.cpp
        ../filesystem/fat32/fat32_lfn.cpp
        ../filesystem/fat32/fat32_time.cpp
        ../lib/path.cpp
        fat32/fat32_tests_main.cpp
)
target_link_libraries(fat32_tests PRIVATE kernel_stubs)
target_compile_options(fat32_tests PRIVATE
        -DUNIT_TEST
        -fno-rtti
        -g
)

# =============================================================================
# Heap tests
# =============================================================================

add_executable(heap_tests
        ../kernel/memory/heap.cpp
        heap/heap_tests_main.cpp
)
target_link_libraries(heap_tests PRIVATE kernel_stubs)
target_compile_options(heap_tests PRIVATE
        -DUNIT_TEST
        -fno-rtti
        -g
        # -fsanitize=address
)
target_include_directories(heap_tests PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/../
        ${PROJECT_ROOT}/include
        heap
)

# =============================================================================
# CTest
# =============================================================================

enable_testing()

# ---- FAT32 ----
add_test(NAME FAT32_FileIO
        COMMAND fat32_tests FAT32_FileIO
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME FAT32_Dir
        COMMAND fat32_tests FAT32_Dir
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME FAT32_FAT
        COMMAND fat32_tests FAT32_FAT
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME FAT32_FS
        COMMAND fat32_tests FAT32_FS
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME FAT32_ALL
        COMMAND fat32_tests
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# ---- Heap ----
add_test(NAME Heap_Malloc
        COMMAND heap_tests Heap_Malloc
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME Heap_Segment
        COMMAND heap_tests Heap_Segment
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME Heap_Aligned
        COMMAND heap_tests Heap_Aligned
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME Heap_Validate
        COMMAND heap_tests Heap_Validate
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME Heap_Edge
        COMMAND heap_tests Heap_Edge
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME Heap_Stress
        COMMAND heap_tests Heap_Stress
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME Heap_ALL
        COMMAND heap_tests
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)