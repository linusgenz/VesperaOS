# arch/CMakeLists.txt
#
# Builds the architecture-specific OBJECT library (arch_lib).
#
# Include rules
# ─────────────
#   PRIVATE  → arch/         (apic.h, idt.h, gdt.h, … – never exported)
#   PUBLIC   → include/      (public API cross-modules depend on)
#
# No other module's source tree appears here.  If kernel/ code needs
# something from arch, it must go through a header in include/vespera/.

find_program(NASM_EXECUTABLE nasm REQUIRED)

collect_cpp(ARCH_CPP_SOURCES ${CMAKE_SOURCE_DIR}/arch)
collect_asm(ARCH_ASM_SOURCES ${CMAKE_SOURCE_DIR}/arch)

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


# Must be declared before add_library so the property takes effect.
set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/arch/x86_64/interrupts/interrupts.cpp
        PROPERTIES COMPILE_FLAGS
        "-mno-red-zone -mgeneral-regs-only -ffreestanding"
)

add_library(arch_lib OBJECT
        ${ARCH_CPP_SOURCES}
        ${ARCH_ASM_SOURCES}
)

add_dependencies(arch_lib ap_trampoline)

apply_kernel_compile_options(arch_lib)
kernel_module_includes(arch_lib ${CMAKE_SOURCE_DIR}/arch)