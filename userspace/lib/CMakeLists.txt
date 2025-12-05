cmake_minimum_required(VERSION 3.16)
project(vesplib C ASM_NASM)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

file(GLOB_RECURSE LIB_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

set(ASM_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/crt0.asm
)

add_library(vesplib STATIC ${LIB_SOURCES} ${ASM_SOURCES})

# NASM Flags korrekt setzen
set_source_files_properties(
        ${ASM_SOURCES}
        PROPERTIES
        LANGUAGE ASM_NASM
        COMPILE_FLAGS "-f elf64"
)

set_target_properties(vesplib PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME "vesplib"
)

target_include_directories(vesplib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(vesplib PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
        $<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>
        $<$<COMPILE_LANGUAGE:C>:-fno-builtin>
        $<$<COMPILE_LANGUAGE:C>:-nostdinc>
        $<$<COMPILE_LANGUAGE:C>:-Wall>
        $<$<COMPILE_LANGUAGE:C>:-Wextra>
        $<$<COMPILE_LANGUAGE:C>:-O0>
        $<$<COMPILE_LANGUAGE:C>:-fPIC>
)