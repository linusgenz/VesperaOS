cmake_minimum_required(VERSION 3.20)

set(ACPICA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/acpica")
set(ACPICA_COMPONENTS "${ACPICA_ROOT}/components")

file(GLOB_RECURSE ACPICA_SOURCES
        "${ACPICA_COMPONENTS}/dispatcher/*.c"
        "${ACPICA_COMPONENTS}/events/*.c"
        "${ACPICA_COMPONENTS}/executer/*.c"
        "${ACPICA_COMPONENTS}/hardware/*.c"
        "${ACPICA_COMPONENTS}/namespace/*.c"
        "${ACPICA_COMPONENTS}/parser/*.c"
        "${ACPICA_COMPONENTS}/resources/*.c"
        "${ACPICA_COMPONENTS}/tables/*.c"
        "${ACPICA_COMPONENTS}/utilities/*.c"
)

list(FILTER ACPICA_SOURCES EXCLUDE REGEX ".*/debugger/.*")
list(FILTER ACPICA_SOURCES EXCLUDE REGEX ".*/disassembler/.*")
list(FILTER ACPICA_SOURCES EXCLUDE REGEX ".*/decompiler/.*")

message(STATUS "ACPICA: ${CMAKE_CURRENT_SOURCE_DIR}")

# ─── acpica: raw C library, no VesperaOS headers ─────────────────────────────

add_library(acpica OBJECT ${ACPICA_SOURCES})

set_target_properties(acpica PROPERTIES LINKER_LANGUAGE C)

set_source_files_properties(${ACPICA_SOURCES} PROPERTIES LANGUAGE C)

target_compile_options(acpica PRIVATE
        -w
        -ffreestanding
        -nostdinc
        -fno-stack-protector
        -fno-pic
        -mno-red-zone
        -mcmodel=kernel
        -O2
)

target_compile_definitions(acpica PRIVATE
        __VESPERA_OS__
        ACPI_MACHINE_WIDTH=64
        ACPI_USE_LOCAL_CACHE=1
        ACPI_USE_BUILTIN_STDARG=1
)

target_include_directories(acpica PUBLIC
        "${ACPICA_ROOT}/include"
)

target_include_directories(acpica PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
)


set(ACPI_KERNEL_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/acpi_osl.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/acpi.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/acpi_subsystem.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/ec.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/madt.cpp"
)

add_library(acpi_kernel OBJECT ${ACPI_KERNEL_SOURCES})

target_compile_options(acpi_kernel PRIVATE
        -ffreestanding
        -fno-stack-protector
        -fno-pic
        -mno-red-zone
        -mcmodel=kernel
        -O2
)

target_compile_definitions(acpi_kernel PRIVATE
        __VESPERA_OS__
        ACPI_MACHINE_WIDTH=64
)

target_include_directories(acpi_kernel PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${ACPICA_ROOT}/include"
        "${CMAKE_BINARY_DIR}"
        "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(acpi_kernel PUBLIC acpica)