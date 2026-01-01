cmake_minimum_required(VERSION 3.16)
project(vesplib C ASM_NASM)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

# --- C Sources ---
set(LIB_SOURCES
        src/channel.c
        src/exec.c
        src/file.c
        src/internal.c
        src/malloc.c
        src/realm.c
        src/reboot.c
        src/stdio.c
        src/stdlib.c
        src/string.c
        src/sysstd.c
        src/urandom.c

        src/sys/ioctl.c
        src/sys/mman.c
)

# --- ASM Sources ---
set(ASM_SOURCES
        src/crt0.asm
)

add_library(vesplib STATIC
        ${LIB_SOURCES}
        ${ASM_SOURCES}
)

set_source_files_properties(
        src/crt0.asm
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