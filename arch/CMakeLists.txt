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


collect_cpp(ARCH_CPP_SOURCES ${CMAKE_SOURCE_DIR}/arch)
collect_asm(ARCH_ASM_SOURCES ${CMAKE_SOURCE_DIR}/arch)

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

apply_kernel_compile_options(arch_lib)
kernel_module_includes(arch_lib ${CMAKE_SOURCE_DIR}/arch)