cmake_minimum_required(VERSION 4.0.2)
project(bootstrap C CXX ASM_NASM)

set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

set(COMMON_FLAGS
        -ffreestanding
        -fno-stack-protector
        -fno-pic
        -fno-pie
        -mno-red-zone
        -mcmodel=kernel
        -Wall
        -Wextra
        -Wno-unused-parameter
)

set(BOOTSTRAP_SOURCES
        bootstrap.cpp
        entry.asm
        jump.asm

        lib/string.cpp

        memory/memory.cpp
        paging/bitmap.cpp
        paging/page_frame_allocator.cpp
        paging/page_map_indexer.cpp
        paging/page_table_manager.cpp
        paging/paging.cpp

        panic/panic.cpp
)

add_executable(bootstrap.elf ${BOOTSTRAP_SOURCES})

set_source_files_properties(entry.asm PROPERTIES
        LANGUAGE ASM_NASM
)

target_include_directories(bootstrap.elf PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

set_source_files_properties(entry.asm PROPERTIES
        LANGUAGE ASM_NASM
        COMPILE_FLAGS "-f elf64"
)

target_compile_options(bootstrap.elf PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -mcmodel=kernel -Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti>
)

target_link_options(bootstrap.elf PRIVATE
        -nostdlib
        -nostartfiles
        -nodefaultlibs
        -T ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
        -z max-page-size=0x1000
)

set_target_properties(bootstrap.elf PROPERTIES
        LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
)
