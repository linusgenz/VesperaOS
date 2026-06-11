# kernel/CMakeLists.txt
#
# Builds:
#   libmath          – freestanding math OBJECT library
#   kernel_core_lib  – core kernel OBJECT library
#   kernel.elf       – final linked kernel image
#                      (consumes all module OBJECT libraries)
#
# Include rules for kernel_core_lib
# ──────────────────────────────────
#   PRIVATE  → kernel/       (internal headers – never exported)
#   PUBLIC   → include/      (cross-module public API only)
#
# arch/, drivers/, filesystem/ are NOT in the include path here.
# If kernel code needs something from those modules it must go
# through a header under include/vespera/.

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/acpi acpi_build)

# ── libmath (compiled with -O2, separate from kernel debug flags) ──
file(GLOB_RECURSE LIBMATH_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../lib/math/*.cpp")

add_library(libmath OBJECT ${LIBMATH_SOURCES})

target_compile_options(libmath PRIVATE
        -w
        -ffreestanding
        -fno-stack-protector
        -fno-pic
        -mno-red-zone
        -mcmodel=kernel
        -O2
)
target_include_directories(libmath PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}
)

# ── Collect kernel core sources ───────────────────────────────────
collect_cpp(KERNEL_CPP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../lib
        ${CMAKE_CURRENT_SOURCE_DIR}/../log
)
collect_asm(KERNEL_ASM_SOURCES
        ${CMAKE_SOURCE_DIR}
)

# Remove sources that belong to separate targets
list(FILTER KERNEL_CPP_SOURCES EXCLUDE REGEX ".*/lib/math/.*")
list(FILTER KERNEL_CPP_SOURCES EXCLUDE REGEX ".*/kernel/acpi/.*")
list(FILTER KERNEL_ASM_SOURCES EXCLUDE REGEX ".*/ap_trampoline\\.asm")

# Remove debug sources that are compiled-out via option
if (NOT ENABLE_DEBUG_SPINLOCK)
    list(FILTER KERNEL_CPP_SOURCES EXCLUDE REGEX ".*/debug/lock_debug\\.cpp")
    list(FILTER KERNEL_CPP_SOURCES EXCLUDE REGEX ".*/debug/deadlock_detector\\.cpp")
endif ()

if (NOT ENABLE_DEBUG_FAULT)
    list(FILTER KERNEL_CPP_SOURCES EXCLUDE REGEX ".*/debug/disasm\\.cpp")
    list(FILTER KERNEL_CPP_SOURCES EXCLUDE REGEX ".*/debug/trace\\.cpp")
    list(FILTER KERNEL_CPP_SOURCES EXCLUDE REGEX ".*/debug/symbols\\.cpp")
endif ()

# ── Version header ────────────────────────────────────────────────
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/kversion.h
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_version_header.sh
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_version_header.sh
)
add_custom_target(version DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/kversion.h)

# ── Embedded PSF font ─────────────────────────────────────────────
set(FONT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets")
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

# ── AP trampoline (separate flat binary → embedded header) ────────
find_program(NASM_EXECUTABLE nasm REQUIRED)

# ── Kernel symbol table (only when ENABLE_DEBUG_FAULT) ───────────
if (ENABLE_DEBUG_FAULT)

    # Bootstrap boilerplate so Stage 1 can compile even before the
    # real symbol header is generated.
    set(BOILERPLATE_HEADER "${CMAKE_BINARY_DIR}/kernel_symbols.h")
    if (NOT EXISTS ${BOILERPLATE_HEADER})
        file(WRITE ${BOILERPLATE_HEADER}
                "unsigned char kernel_map[] = {};\nunsigned int kernel_map_len = 0;\n")
    endif ()

    # ── Stage 1 kernel (used only to extract symbols) ─────────────
    add_executable(kernel_stage1
            ${KERNEL_CPP_SOURCES}
            ${KERNEL_ASM_SOURCES}
            ${CMAKE_BINARY_DIR}/font.o
            $<TARGET_OBJECTS:acpica>
            $<TARGET_OBJECTS:acpi_kernel>
            $<TARGET_OBJECTS:libmath>
            $<TARGET_OBJECTS:arch_lib>
            $<TARGET_OBJECTS:drivers_lib>
            $<TARGET_OBJECTS:filesystem_lib>
    )
    set_target_properties(kernel_stage1 PROPERTIES
            OUTPUT_NAME "kernel_stage1"
            LINK_FLAGS "-T ${CMAKE_CURRENT_SOURCE_DIR}/../linker.ld -nostdlib -static -mcmodel=kernel"
    )
    apply_kernel_compile_options(kernel_stage1)
    kernel_module_includes(kernel_stage1 ${CMAKE_CURRENT_SOURCE_DIR})
    #target_include_directories(kernel_stage1 PRIVATE ${CMAKE_SOURCE_DIR})

    add_dependencies(kernel_stage1 embedded_font)

    # Generate kernel.map from stage-1 symbols
    add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/kernel.map
            COMMAND ${CMAKE_NM} -n $<TARGET_FILE:kernel_stage1> > ${CMAKE_BINARY_DIR}/kernel.map
            DEPENDS kernel_stage1
            COMMENT "Generating kernel.map"
    )
    add_custom_target(kernel_map DEPENDS ${CMAKE_BINARY_DIR}/kernel.map)

    # Embed kernel.map into kernel_symbols.h
    add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/kernel_symbols.h
            COMMAND xxd -i ${CMAKE_BINARY_DIR}/kernel.map
            | sed 's/unsigned char .*\\[/inline unsigned char kernel_map[/'
            | sed 's/unsigned int .*_len/inline unsigned int kernel_map_len/'
            > ${CMAKE_BINARY_DIR}/kernel_symbols.h
            DEPENDS ${CMAKE_BINARY_DIR}/kernel.map
            COMMENT "Embedding kernel.map → kernel_symbols.h"
    )
    add_custom_target(kernel_symbols_header
            DEPENDS ${CMAKE_BINARY_DIR}/kernel_symbols.h
    )

endif ()

# ─────────────────────────────────────────────────────────────────
# kernel_core_lib  –  kernel internals as OBJECT library
# ─────────────────────────────────────────────────────────────────
if (ENABLE_DEBUG_FAULT)
    set(KERNEL_EXTRA ${CMAKE_BINARY_DIR}/kernel_symbols.h)
else ()
    set(KERNEL_EXTRA "")
endif ()

add_library(kernel_core_lib OBJECT
        ${KERNEL_CPP_SOURCES}
        ${KERNEL_ASM_SOURCES}
        ${KERNEL_EXTRA}           # empty string is ignored by CMake
)

apply_kernel_compile_options(kernel_core_lib)
kernel_module_includes(kernel_core_lib ${CMAKE_SOURCE_DIR})

# kernel/ needs access to its own root for intra-kernel includes
target_include_directories(kernel_core_lib PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}       # e.g. #include "kernel/cpu/cpu.h" within kernel
)

add_dependencies(kernel_core_lib ap_trampoline embedded_font version)
if (ENABLE_DEBUG_FAULT)
    add_dependencies(kernel_core_lib kernel_symbols_header)
endif ()

# ─────────────────────────────────────────────────────────────────
# kernel.elf  –  final linked image
#
# Links ALL module OBJECT libraries.  No additional source dirs are
# added to the include path here; everything is inherited transitively
# via the PUBLIC include dirs of each OBJECT library.
# ─────────────────────────────────────────────────────────────────
add_executable(kernel.elf
        ${CMAKE_BINARY_DIR}/font.o
        $<TARGET_OBJECTS:kernel_core_lib>
        $<TARGET_OBJECTS:arch_lib>
        $<TARGET_OBJECTS:drivers_lib>
        $<TARGET_OBJECTS:filesystem_lib>
        $<TARGET_OBJECTS:acpica>
        $<TARGET_OBJECTS:acpi_kernel>
        $<TARGET_OBJECTS:libmath>
)

set_target_properties(kernel.elf PROPERTIES
        OUTPUT_NAME "kernel.elf"
        LINK_FLAGS "-T ${CMAKE_SOURCE_DIR}/linker.ld -nostdlib -static -mcmodel=kernel"
)

# kernel.elf itself needs no include dirs – it has no source files,
# only OBJECT inputs whose include paths were already resolved.
# The PUBLIC dirs from each OBJECT lib are not needed at link time.

add_dependencies(kernel.elf embedded_font ap_trampoline version)
if (ENABLE_DEBUG_FAULT)
    add_dependencies(kernel.elf kernel_symbols_header)
endif ()