# kernel/CMakeLists.txt

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

# Set C++ specific flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffreestanding -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fshort-wchar -fno-exceptions -fno-rtti -g -O0 -fno-inline -Wall")

set(KERNEL_DIR ${CMAKE_SOURCE_DIR})
set(KERNEL_SRC_DIR ${KERNEL_DIR}/kernel)
set(ARCH_SRC_DIR ${KERNEL_DIR}/arch)
set(DRIVERS_SRC_DIR ${KERNEL_DIR}/drivers)
set(FS_SRC_DIR ${KERNEL_DIR}/filesystem)
set(LIB_SRC_DIR ${KERNEL_DIR}/lib)
set(LOG_SRC_DIR ${KERNEL_DIR}/log)
set(INCLUDE_SRC_DIR ${KERNEL_DIR}/include)

function(collect_cpp OUT_VAR)
    set(ALL_CPP_FILES "")
    foreach (dir ${ARGN})
        file(GLOB_RECURSE DIR_CPP_FILES
                "${dir}/*.cpp"
                "${dir}/*.cc"
        )
        list(APPEND ALL_CPP_FILES ${DIR_CPP_FILES})
    endforeach ()
    list(FILTER ALL_CPP_FILES EXCLUDE REGEX "${CMAKE_BINARY_DIR}/.*")
    set(${OUT_VAR} ${ALL_CPP_FILES} PARENT_SCOPE)
endfunction()

function(collect_asm OUT_VAR)
    set(ALL_ASM_FILES "")
    foreach (dir ${ARGN})
        file(GLOB_RECURSE DIR_ASM_FILES
                "${dir}/*.asm"
        )
        list(APPEND ALL_ASM_FILES ${DIR_ASM_FILES})
    endforeach ()
    list(FILTER ALL_ASM_FILES EXCLUDE REGEX "${CMAKE_BINARY_DIR}/.*")
    set(${OUT_VAR} ${ALL_ASM_FILES} PARENT_SCOPE)
endfunction()

collect_cpp(KERNEL_CPP_SOURCES
        ${KERNEL_SRC_DIR}
        ${ARCH_SRC_DIR}
        ${DRIVERS_SRC_DIR}
        ${FS_SRC_DIR}
        ${LIB_SRC_DIR}
        ${LOG_SRC_DIR}
)

collect_asm(KERNEL_ASM_SOURCES
        ${KERNEL_SRC_DIR}
        ${ARCH_SRC_DIR}
)

# Kernel C++ Dateien
message(STATUS "Kernel C++ files:")
foreach (file_path IN LISTS KERNEL_CPP_SOURCES)
    message(STATUS "  ${file_path}")
endforeach ()

# Kernel ASM Dateien
message(STATUS "Kernel ASM files:")
foreach (file_path IN LISTS KERNEL_ASM_SOURCES)
    message(STATUS "  ${file_path}")
endforeach ()

list(LENGTH KERNEL_CPP_SOURCES NUM_CPP)
message(STATUS "Number of Kernel C++ files: ${NUM_CPP}")

list(LENGTH KERNEL_ASM_SOURCES NUM_ASM)
message(STATUS "Number of Kernel ASM files: ${NUM_ASM}")


set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/arch/x86_64/interrupts/interrupts.cpp
        PROPERTIES COMPILE_FLAGS "-mno-red-zone -mgeneral-regs-only -ffreestanding"
)

set(BOILERPLATE_HEADER "${CMAKE_BINARY_DIR}/kernel_symbols.h")

if (NOT EXISTS ${BOILERPLATE_HEADER})
    file(WRITE ${BOILERPLATE_HEADER} "unsigned char kernel_map[] = {};\nunsigned int kernel_map_len = 0;\n")
endif ()

# kernel_stage1 (used for generating kernel map)
add_executable(kernel_stage1
        ${KERNEL_CPP_SOURCES}
        ${KERNEL_ASM_SOURCES}
)
set_target_properties(kernel_stage1 PROPERTIES
        OUTPUT_NAME "kernel_stage1"
        LINK_FLAGS "-T ${CMAKE_SOURCE_DIR}/linker.ld -nostdlib -static"
)

target_include_directories(kernel_stage1 PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/kernel/include
        ${CMAKE_BINARY_DIR}
)

# Kernel symbols embedding
add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/kernel_symbols.h
        COMMAND xxd -i ${CMAKE_BINARY_DIR}/kernel.map |
        sed 's/unsigned char .*\\[/inline unsigned char kernel_map[/' |
        sed 's/unsigned int .*_len/inline unsigned int kernel_map_len/'
        > ${CMAKE_BINARY_DIR}/kernel_symbols.h
        DEPENDS ${CMAKE_BINARY_DIR}/kernel.map
        COMMENT "Embedding kernel.map as kernel_symbols.h"
)
add_custom_target(kernel_symbols_header
        DEPENDS ${CMAKE_BINARY_DIR}/kernel_symbols.h
)


add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/kernel.map
        COMMAND ${CMAKE_NM} -n $<TARGET_FILE:kernel_stage1> > ${CMAKE_BINARY_DIR}/kernel.map
        DEPENDS kernel_stage1
        COMMENT "Generating kernel.map from kernel_stage1"
)
add_custom_target(kernel_map
        DEPENDS ${CMAKE_BINARY_DIR}/kernel.map
)

# Final kernel
add_executable(kernel.elf
        ${KERNEL_CPP_SOURCES}
        ${KERNEL_ASM_SOURCES}
        ${CMAKE_BINARY_DIR}/kernel_symbols.h
)
set_target_properties(kernel.elf PROPERTIES
        OUTPUT_NAME "kernel.elf"
        LINK_FLAGS "-T ${CMAKE_SOURCE_DIR}/linker.ld -nostdlib -static"
)

target_include_directories(kernel.elf PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/kernel/include
        ${CMAKE_BINARY_DIR}
)

add_dependencies(kernel.elf kernel_symbols_header)