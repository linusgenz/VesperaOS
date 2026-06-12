cmake_minimum_required(VERSION 3.24)
project(VesperaOS C CXX)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

set(VESPERA_UAPI_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/uapi)

include(KernelTargetDefaults.cmake)

if(NOT DEFINED VESPERA_SYSROOT)
    if(DEFINED ENV{VESPERA_SYSROOT})
        set(VESPERA_SYSROOT "$ENV{VESPERA_SYSROOT}")
    else()
        set(VESPERA_SYSROOT "${CMAKE_SOURCE_DIR}/../VesperaSysroot")
    endif()
endif()
get_filename_component(VESPERA_SYSROOT "${VESPERA_SYSROOT}" ABSOLUTE)
message(STATUS "VESPERA_SYSROOT: ${VESPERA_SYSROOT}")

message(STATUS "=== Compile Options ===")
add_debug_option(ENABLE_DEBUG_FAULT    "Enable fault/callstack debugger" ON)
add_debug_option(ENABLE_DEBUG_SPINLOCK "Enable spinlock/deadlock debugger" ON)
add_debug_option(ENABLE_ELF_LOGGING    "Enable logging for ELF module"   OFF)
message(STATUS "=====================")

# ────────────────────────────────────────────────────────────────
# Test build (early return)
# ────────────────────────────────────────────────────────────────
option(VESPERA_BUILD_TESTS "Build unit tests" OFF)

if(VESPERA_BUILD_TESTS)
    enable_testing()
    set(CMAKE_CXX_FLAGS "-g -O0 -Wall -Wextra -fno-rtti -fno-exceptions")
    set(CMAKE_C_FLAGS   "-g -O0 -Wall")
    add_subdirectory(tests)
    return()
else()
    enable_language(ASM_NASM)
endif()

set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

add_subdirectory(arch)
add_subdirectory(drivers)
add_subdirectory(filesystem)
add_subdirectory(kernel)        # defines kernel.elf + version
add_subdirectory(userspace/lib) # defines sysroot_install

# ────────────────────────────────────────────────────────────────
# Limine Bootloader
# ────────────────────────────────────────────────────────────────
set(LIMINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/limine)

add_custom_target(bootloader
        DEPENDS kernel.elf
        COMMAND make -C ${LIMINE_DIR}
        COMMENT "Building Limine bootloader"
)

# Kernel ELF only.
add_custom_target(os-build-kernel
        DEPENDS kernel.elf
        COMMENT "Building kernel only"
)

# Userspace libraries → sysroot.
add_custom_target(os-build-libs
        DEPENDS sysroot_install
        COMMENT "Building userspace libraries → sysroot"
)

# Kernel + libs (no disk, no apps).
add_custom_target(os-build
        DEPENDS kernel.elf sysroot_install
        COMMENT "Building kernel + userspace libs"
)

add_custom_target(os-sysroot-install
        DEPENDS sysroot_install
)

add_custom_target(os-clean-all
        COMMAND ${CMAKE_COMMAND} -E echo "Cleaning VesperaOS build artifacts..."
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/*"
        COMMAND ${CMAKE_COMMAND} -E rm -f  "${CMAKE_SOURCE_DIR}/splash.h"
        COMMAND ${CMAKE_COMMAND} -E rm -f  "${CMAKE_SOURCE_DIR}/kernel/kversion.h"
        COMMAND ${CMAKE_COMMAND} -E echo "Clean complete."
        COMMENT "Removing build directory and generated headers"
)