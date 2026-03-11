# kernel/CMakeLists.txt

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

set(KERNEL_DIR ${CMAKE_SOURCE_DIR})
set(KERNEL_SRC_DIR ${KERNEL_DIR}/kernel)
set(ARCH_SRC_DIR ${KERNEL_DIR}/arch)
set(DRIVERS_SRC_DIR ${KERNEL_DIR}/drivers)
set(FS_SRC_DIR ${KERNEL_DIR}/filesystem)
set(LIB_SRC_DIR ${KERNEL_DIR}/lib)
set(LOG_SRC_DIR ${KERNEL_DIR}/log)
set(INCLUDE_SRC_DIR ${KERNEL_DIR}/include)


function(add_debug_option name description default_value)
    option(${name} ${description} ${default_value})

    if (${name})
        add_compile_definitions(${name}=1)
        set(status "ENABLED")
    else ()
        add_compile_definitions(${name}=0)
        set(status "DISABLED")
    endif ()

    message(STATUS "  ${name}: ${status}")
endfunction()

message(STATUS "=== Compile Options ===")
add_debug_option(ENABLE_DEBUG_FAULT "Enable fault/callstack debugger" ON)
add_debug_option(ENABLE_DEBUG_SPINLOCK "Enable spinlock/deadlock debugger" ON)
add_debug_option(ENABLE_ELF_LOGGING "Enable logging for ELF module" OFF)
message(STATUS "=====================")

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

list(FILTER KERNEL_ASM_SOURCES EXCLUDE REGEX ".*/ap_trampoline\\.asm")

# REMOVE debug source files when disabled
if (NOT ENABLE_DEBUG_SPINLOCK)
    list(FILTER KERNEL_CPP_SOURCES EXCLUDE REGEX ".*/debug/lock_debug.cpp")
    list(FILTER KERNEL_CPP_SOURCES EXCLUDE REGEX ".*/debug/deadlock_detector.cpp")
endif ()

if (NOT ENABLE_DEBUG_FAULT)
    list(FILTER KERNEL_CPP_SOURCES EXCLUDE REGEX ".*/debug/disasm.cpp")
    list(FILTER KERNEL_CPP_SOURCES EXCLUDE REGEX ".*/debug/trace.cpp")
    list(FILTER KERNEL_CPP_SOURCES EXCLUDE REGEX ".*/debug/symbols.cpp")
endif ()

set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/arch/x86_64/interrupts/interrupts.cpp
        PROPERTIES COMPILE_FLAGS "-mno-red-zone -mgeneral-regs-only -ffreestanding"
)

# version header
add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/kernel/kversion.h
        COMMAND bash ${CMAKE_SOURCE_DIR}/generate_version_header.sh
        DEPENDS ${CMAKE_SOURCE_DIR}/generate_version_header.sh
)
add_custom_target(version DEPENDS ${CMAKE_SOURCE_DIR}/kernel/kversion.h)


set(FONT_DIR "${CMAKE_SOURCE_DIR}/assets")
set(FONT_FILE "zap-light24.psf")
set(FONT_PSF "${FONT_DIR}/${FONT_FILE}")


add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/font.o
        COMMAND objcopy
        -I binary
        -O elf64-x86-64
        -B i386:x86-64
        --redefine-sym _binary_zap_light24_psf_start=psf_font_start
        --redefine-sym _binary_zap_light24_psf_end=psf_font_end
        --redefine-sym _binary_zap_light24_psf_size=psf_font_size
        --rename-section .data=.data.font,alloc,load,readonly,data,contents
        ${FONT_FILE}
        ${CMAKE_BINARY_DIR}/font.o
        DEPENDS ${FONT_PSF}
        WORKING_DIRECTORY ${FONT_DIR}
        COMMENT "Embedding PSF font (${FONT_FILE}) into kernel binary"
)

add_custom_target(embedded_font DEPENDS ${CMAKE_BINARY_DIR}/font.o)

# ────────────────────────────────────────────────────────────────
#   AP TRAMPOLINE - separate flat binary
# ────────────────────────────────────────────────────────────────

find_program(NASM_EXECUTABLE nasm REQUIRED)

set(AP_TRAMPOLINE_ASM ${CMAKE_SOURCE_DIR}/kernel/cpu/ap_trampoline.asm)
set(AP_TRAMPOLINE_BIN ${CMAKE_BINARY_DIR}/ap_trampoline.bin)
set(AP_TRAMPOLINE_HEADER ${CMAKE_BINARY_DIR}/ap_trampoline_blob.h)

add_custom_command(
        OUTPUT ${AP_TRAMPOLINE_BIN}
        COMMAND ${NASM_EXECUTABLE} -f bin
        -o ${AP_TRAMPOLINE_BIN}
        ${AP_TRAMPOLINE_ASM}
        DEPENDS ${AP_TRAMPOLINE_ASM}
        COMMENT "Assembling AP trampoline flat binary"
)

add_custom_command(
        OUTPUT ${AP_TRAMPOLINE_HEADER}
        COMMAND xxd -i ap_trampoline.bin
        | sed 's/unsigned char ap_trampoline_bin/inline unsigned char ap_trampoline_bin/'
        | sed 's/unsigned int ap_trampoline_bin_len/inline unsigned int ap_trampoline_bin_len/'
        > ${AP_TRAMPOLINE_HEADER}
        DEPENDS ${AP_TRAMPOLINE_BIN}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating AP trampoline header"
)

add_custom_target(ap_trampoline DEPENDS ${AP_TRAMPOLINE_HEADER})

#
# ────────────────────────────────────────────────────────────────
#   CONDITIONAL KERNEL SYMBOLS GENERATION  (ONLY IF DEBUG_FAULT)
# ────────────────────────────────────────────────────────────────
#

if (ENABLE_DEBUG_FAULT)

    set(BOILERPLATE_HEADER "${CMAKE_BINARY_DIR}/kernel_symbols.h")

    if (NOT EXISTS ${BOILERPLATE_HEADER})
        file(WRITE ${BOILERPLATE_HEADER}
                "unsigned char kernel_map[] = {};\nunsigned int kernel_map_len = 0;\n")
    endif ()

    # Stage1 kernel
    add_executable(kernel_stage1
            ${KERNEL_CPP_SOURCES}
            ${KERNEL_ASM_SOURCES}
            ${CMAKE_BINARY_DIR}/font.o
    )
    set_target_properties(kernel_stage1 PROPERTIES
            OUTPUT_NAME "kernel_stage1"
            LINK_FLAGS "-T ${CMAKE_SOURCE_DIR}/linker.ld -nostdlib -static -mcmodel=kernel"
    )

    target_include_directories(kernel_stage1 PUBLIC
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_BINARY_DIR}
            ${CMAKE_SOURCE_DIR}/limine
    )

    add_dependencies(kernel_stage1 embedded_font ap_trampoline)

    # generate kernel.map
    add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/kernel.map
            COMMAND ${CMAKE_NM} -n $<TARGET_FILE:kernel_stage1> > ${CMAKE_BINARY_DIR}/kernel.map
            DEPENDS kernel_stage1
            COMMENT "Generating kernel.map"
    )
    add_custom_target(kernel_map DEPENDS ${CMAKE_BINARY_DIR}/kernel.map)

    # embed kernel.map → kernel_symbols.h
    add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/kernel_symbols.h
            COMMAND xxd -i ${CMAKE_BINARY_DIR}/kernel.map |
            sed 's/unsigned char .*\\[/inline unsigned char kernel_map[/' |
            sed 's/unsigned int .*_len/inline unsigned int kernel_map_len/'
            > ${CMAKE_BINARY_DIR}/kernel_symbols.h
            DEPENDS ${CMAKE_BINARY_DIR}/kernel.map
            COMMENT "Embedding kernel.map"
    )
    add_custom_target(kernel_symbols_header
            DEPENDS ${CMAKE_BINARY_DIR}/kernel_symbols.h
    )

endif ()

#
# ────────────────────────────────────────────────────────────────
#   FINAL KERNEL
# ────────────────────────────────────────────────────────────────
#

if (ENABLE_DEBUG_FAULT)
    set(KERNEL_EXTRA ${CMAKE_BINARY_DIR}/kernel_symbols.h)
else ()
    set(KERNEL_EXTRA "")
endif ()

add_executable(kernel.elf
        ${KERNEL_CPP_SOURCES}
        ${KERNEL_ASM_SOURCES}
        ${KERNEL_EXTRA}
        ${CMAKE_BINARY_DIR}/font.o
)

set_target_properties(kernel.elf PROPERTIES
        OUTPUT_NAME "kernel.elf"
        LINK_FLAGS "-T ${CMAKE_SOURCE_DIR}/linker.ld -nostdlib -static -mcmodel=kernel"
)

target_include_directories(kernel.elf PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/limine
)

if (ENABLE_DEBUG_FAULT)
    add_dependencies(kernel.elf kernel_symbols_header embedded_font ap_trampoline version)
else ()
    add_dependencies(kernel.elf embedded_font ap_trampoline version)
endif ()