cmake_minimum_required(VERSION 3.20)

project(VesperaOS C CXX ASM_NASM)

enable_language(ASM_NASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_source_files_properties(${ASM_SOURCES} PROPERTIES LANGUAGE ASM_NASM)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

# Compiler-Flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffreestanding -fno-stack-protector -mno-red-zone -fshort-wchar -fno-exceptions -fno-rtti -g -O0 -Wall")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -ffreestanding -fno-stack-protector -mno-red-zone -fshort-wchar -fno-exceptions -g -O0 -Wall")

# Includes
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/kernel/include
)

# Quellcodedateien
file(GLOB_RECURSE CPP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/**/*.cpp
)

set(FILTERED_CPP_SOURCES "")
foreach(file ${ALL_CPP_SOURCES})
    if (NOT file MATCHES "/gnu%-efi/" AND NOT file MATCHES "/bin/")
        list(APPEND FILTERED_CPP_SOURCES ${file})
    endif()
endforeach()

file(GLOB_RECURSE ASM_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/arch/**/*.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/kernel/**/*.asm
)

file(GLOB_RECURSE C_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/*.c
)

# Spezialfall: interrupts.cpp
set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64/interrupts/interrupts.cpp
        PROPERTIES COMPILE_FLAGS "-mno-red-zone -mgeneral-regs-only -ffreestanding"
)

add_executable(kernel.elf ${FILTERED_CPP_SOURCES} ${ASM_SOURCES} ${C_SOURCES})

set_target_properties(kernel.elf PROPERTIES
        LINK_FLAGS "-T ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld -nostdlib -static"
)
