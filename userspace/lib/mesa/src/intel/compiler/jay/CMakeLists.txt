# lib/mesa/src/intel/compiler/jay/CMakeLists.txt

set(JAY_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(JAY_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR})

# 1. jay_opcodes_gen.py -> jay_opcodes.c & jay_opcodes.h
set(JAY_OPCODES_C ${JAY_GENERATED_DIR}/jay_opcodes.c)
set(JAY_OPCODES_H ${JAY_GENERATED_DIR}/jay_opcodes.h)
add_custom_command(
        OUTPUT ${JAY_OPCODES_C} ${JAY_OPCODES_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${JAY_DIR}/jay_opcodes_gen.py
        --code ${JAY_OPCODES_C}
        --header ${JAY_OPCODES_H}
        DEPENDS ${JAY_DIR}/jay_opcodes_gen.py
        ${JAY_DIR}/jay_opcodes.py
        COMMENT "Generating jay_opcodes.c and jay_opcodes.h"
        VERBATIM
)

# 2. jay_extra_info.h.py -> jay_extra_info.h
set(JAY_EXTRA_INFO_H ${JAY_GENERATED_DIR}/jay_extra_info.h)
add_custom_command(
        OUTPUT ${JAY_EXTRA_INFO_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${JAY_DIR}/jay_extra_info.h.py ${JAY_EXTRA_INFO_H}
        DEPENDS ${JAY_DIR}/jay_extra_info.h.py
        ${JAY_DIR}/jay_opcodes.py
        COMMENT "Generating jay_extra_info.h"
        VERBATIM
)

# 3. jay_builder_opcodes.h.py -> jay_builder_opcodes.h
set(JAY_BUILDER_OPCODES_H ${JAY_GENERATED_DIR}/jay_builder_opcodes.h)
add_custom_command(
        OUTPUT ${JAY_BUILDER_OPCODES_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${JAY_DIR}/jay_builder_opcodes.h.py ${JAY_BUILDER_OPCODES_H}
        DEPENDS ${JAY_DIR}/jay_builder_opcodes.h.py
        ${JAY_DIR}/jay_opcodes.py
        COMMENT "Generating jay_builder_opcodes.h"
        VERBATIM
)

# 4. jay_nir_algebraic.py -> jay_nir_algebraic.c
set(JAY_NIR_ALGEBRAIC_C ${JAY_GENERATED_DIR}/jay_nir_algebraic.c)
add_custom_command(
        OUTPUT ${JAY_NIR_ALGEBRAIC_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${JAY_DIR}/jay_nir_algebraic.py
        ${JAY_NIR_ALGEBRAIC_C}
        -p ${MESA_ROOT}/src/compiler/nir
        DEPENDS ${JAY_DIR}/jay_nir_algebraic.py
        COMMENT "Generating jay_nir_algebraic.c"
        VERBATIM
)

add_custom_target(mesa_intel_compiler_jay_headers
        DEPENDS
        ${JAY_OPCODES_H}
        ${JAY_EXTRA_INFO_H}
        ${JAY_BUILDER_OPCODES_H}
)

# ---------------------------------------------------------------------------
# Library Target Sources
# ---------------------------------------------------------------------------

set(MESA_INTEL_COMPILER_JAY_SOURCES
        jay_assign_accumulators.c
        jay_from_nir.c
        jay_liveness.c
        jay_lower_flags.c
        jay_lower_helpers.c
        jay_lower_post_ra.c
        jay_lower_post_sched.c
        jay_lower_pre_ra.c
        jay_lower_scoreboard.c
        jay_lower_spill.c
        jay_nir.c
        jay_opt_dead_code.c
        jay_opt_postra_vectorize.c
        jay_opt_predicate.c
        jay_opt_propagate.c
        jay_partition.c
        jay_performance.c
        jay_print.c
        jay_prog_data.c
        jay_register_allocate.c
        jay_schedule.c
        jay_simd_width.c
        jay_spill.c
        jay_stride.c
        jay_to_binary.c
        jay_validate.c
        jay_validate_ra.c

        # Generierte C-Quelldateien
        ${JAY_OPCODES_C}
        ${JAY_NIR_ALGEBRAIC_C}
)

add_library(mesa_intel_compiler_jay STATIC ${MESA_INTEL_COMPILER_JAY_SOURCES})

target_include_directories(mesa_intel_compiler_jay
        PUBLIC
        ${JAY_DIR}
        ${JAY_DIR}/..
        ${JAY_GENERATED_DIR}
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_link_libraries(mesa_intel_compiler_jay
        PUBLIC
        mesa_util
        mesa_intel_dev
        mesa_intel_compiler_nir
        mesa_intel_compiler_gen
        mesa_intel_mda
        mesa_nir
)

target_compile_options(mesa_intel_compiler_jay
        PRIVATE
        ${MESA_COMMON_C_FLAGS}
)

add_dependencies(mesa_intel_compiler_jay
        sysroot_headers
        mesa_intel_compiler_gen
        mesa_intel_compiler_jay_headers
)