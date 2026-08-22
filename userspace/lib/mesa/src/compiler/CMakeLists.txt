# ════════════════════════════════════════════════════════════════
# lib/mesa/src/compiler/CMakeLists.txt
#
# Modeled on upstream src/compiler/meson.build. This layer produces
# libcompiler (glsl_types, shader_enums, generated builtin_types +
# ir_expression_operation) and then descends into nir/ — mirroring
# meson's subdir('nir') at the end of the file, which is why this
# CMakeLists add_subdirectory(nir)'s itself rather than the parent
# lib/mesa/CMakeLists.txt doing it directly. This keeps the
# dependency direction (nir needs idep_compiler) expressed the same
# way meson expresses it: nir is a child scope of compiler, not a
# sibling.
#
# NOT ported from meson.build: subdir('spirv'), subdir('clc'),
# subdir('glsl') [note: this is src/compiler/glsl, the GLSL FRONTEND
# compiler — unrelated to files_libcompiler's glsl_types.c above,
# which is shared IR type infrastructure], subdir('isaspec'),
# subdir('rust'). None of those are needed for iris/NIR alone; add
# them as their own layers when something downstream actually
# requires them (spirv is likely needed once Vulkan/SPIR-V input
# matters — not for a GL/GLES-only iris path).
# ════════════════════════════════════════════════════════════════

set(COMPILER_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# ────────────────────────────────────────────────────────────────
# builtin_types.h / builtin_types.c — meson.build lines 8–23.
# builtin_types_h.py depends on u_format_gen_h (line 14) — a real
# ordering dependency, not incidental: builtin_types_h.py reads
# format enum data that u_format_table.py generates. That target
# already exists from the util layer (lib/mesa/src/util/CMakeLists.txt);
# we depend on it by name below rather than re-deriving the path.
# ────────────────────────────────────────────────────────────────

set(COMPILER_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# U_FORMAT_GEN_H is propagated here via PARENT_SCOPE from
# lib/mesa/src/util/CMakeLists.txt (see util_cmakelists_patch_snippet.txt) —
# both util/ and compiler/ are direct children of lib/mesa/CMakeLists.txt,
# and util's add_subdirectory() runs first, so by the time this file
# executes the variable is already sitting in the shared parent scope
# that this directory inherits from. Fail loudly instead of silently
# building builtin_types.h with a broken/empty DEPENDS if that patch
# hasn't been applied yet.
if(NOT DEFINED U_FORMAT_GEN_H OR U_FORMAT_GEN_H STREQUAL "")
    message(FATAL_ERROR
            "U_FORMAT_GEN_H is not set. lib/mesa/src/util/CMakeLists.txt "
            "must propagate it via 'set(U_FORMAT_GEN_H ${U_FORMAT_GEN_H} PARENT_SCOPE)' "
            "after generating u_format_gen.h, and src/util must be "
            "add_subdirectory()'d before src/compiler in lib/mesa/CMakeLists.txt."
    )
endif()

set(BUILTIN_TYPES_H ${COMPILER_GEN_DIR}/builtin_types.h)
add_custom_command(
        OUTPUT ${BUILTIN_TYPES_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${COMPILER_ROOT}/builtin_types_h.py ${BUILTIN_TYPES_H}
        DEPENDS ${COMPILER_ROOT}/builtin_types_h.py ${COMPILER_ROOT}/builtin_types.py ${U_FORMAT_GEN_H}
        COMMENT "Generating compiler/builtin_types.h"
        VERBATIM
)

set(BUILTIN_TYPES_C ${COMPILER_GEN_DIR}/builtin_types.c)
add_custom_command(
        OUTPUT ${BUILTIN_TYPES_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${COMPILER_ROOT}/builtin_types_c.py ${BUILTIN_TYPES_C}
        DEPENDS ${COMPILER_ROOT}/builtin_types_c.py ${COMPILER_ROOT}/builtin_types.py
        COMMENT "Generating compiler/builtin_types.c"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# ir_expression_operation.h — meson.build lines 39–45. Note the
# 'enum' positional arg to the script (command includes it) — this
# is NOT optional, the script emits different output depending on
# the mode argument; 'enum' is the only mode meson.build ever uses
# for this custom_target.
# ────────────────────────────────────────────────────────────────
set(IR_EXPRESSION_OPERATION_H ${COMPILER_GEN_DIR}/ir_expression_operation.h)
add_custom_command(
        OUTPUT ${IR_EXPRESSION_OPERATION_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${COMPILER_ROOT}/glsl/ir_expression_operation.py enum > ${IR_EXPRESSION_OPERATION_H}
        DEPENDS ${COMPILER_ROOT}/glsl/ir_expression_operation.py
        COMMENT "Generating compiler/ir_expression_operation.h"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# files_libcompiler — meson.build lines 30–37. Small, fixed list,
# no glob needed.
# ────────────────────────────────────────────────────────────────
set(COMPILER_CORE_SOURCES
        ${COMPILER_ROOT}/glsl_types.c
        ${COMPILER_ROOT}/shader_enums.c
)

add_library(mesa_compiler STATIC
        ${COMPILER_CORE_SOURCES}
        ${BUILTIN_TYPES_H}
        ${BUILTIN_TYPES_C}
        ${IR_EXPRESSION_OPERATION_H}
)

target_include_directories(mesa_compiler
        PUBLIC
        ${COMPILER_ROOT}                  # inc_compiler equivalent
        ${MESA_ROOT}/include               # inc_include equivalent
        ${COMPILER_GEN_DIR}                 # for #include "builtin_types.h" / "ir_expression_operation.h"
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_compiler PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_C_FLAGS}>
)

target_link_libraries(mesa_compiler PUBLIC mesa_util)

add_dependencies(mesa_compiler sysroot_headers mesa_util)

# ────────────────────────────────────────────────────────────────
# subdir('nir') — meson.build line 64. See header comment: nir is
# nested here (not a sibling add_subdirectory from lib/mesa/CMakeLists.txt)
# to mirror the real dependency direction — nir consumes idep_compiler,
# which only exists once this CMakeLists has run.
# ────────────────────────────────────────────────────────────────
add_subdirectory(nir)
add_subdirectory(spirv)
add_subdirectory(glsl)