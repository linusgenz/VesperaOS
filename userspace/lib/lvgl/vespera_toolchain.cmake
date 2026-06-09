# Bezeichne das Ziel-System (verhindert, dass CMake Linux-Annahmen trifft)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_CXX_STANDARD 20)

if(NOT DEFINED VESPERA_SYSROOT)
    if(DEFINED ENV{VESPERA_SYSROOT})
        set(VESPERA_SYSROOT "$ENV{VESPERA_SYSROOT}")
    else()
        set(VESPERA_SYSROOT "${CMAKE_SOURCE_DIR}/../../../../VesperaSysroot")
    endif()
endif()

get_filename_component(VESPERA_SYSROOT "${VESPERA_SYSROOT}" ABSOLUTE)
message(STATUS "Using VESPERA_SYSROOT: ${VESPERA_SYSROOT}")

# Compiler definieren
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Flags, damit für ein eigenes OS ohne Standard-Standardbibliothek gebaut wird
set(VESPERA_FLAGS "-ffreestanding -nostdlib -nostdinc -fno-stack-protector -fPIE")

set(CMAKE_C_FLAGS "${VESPERA_FLAGS} -isystem ${VESPERA_SYSROOT}/usr/include" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${VESPERA_FLAGS} -fno-exceptions -fno-rtti -nostdinc++ -isystem ${VESPERA_SYSROOT}/usr/include -isystem ${VESPERA_SYSROOT}/usr/include/c++" CACHE STRING "" FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
