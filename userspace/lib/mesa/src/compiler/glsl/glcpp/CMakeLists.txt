# ════════════════════════════════════════════════════════════════
# lib/mesa/src/compiler/glsl/glcpp/CMakeLists.txt
#
# 1:1-Uebersetzung von upstream src/compiler/glsl/glcpp/meson.build.
#
# NICHT portiert: glcpp (Standalone-CLI-Tool, meson.build lines
# 60–69) und die glcpp_test-Suite (lines 71–89) -- analog zu allen
# anderen Standalone-Tools/Tests im Baum (spirv2nir, glsl_compiler).
#
# Eingebunden von: lib/mesa/src/compiler/glsl/CMakeLists.txt via
# add_subdirectory(glcpp), VOR dem eigenen add_library(mesa_glsl ...),
# da mesa_glsl gegen mesa_glcpp linkt.
# ════════════════════════════════════════════════════════════════

set(GLCPP_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(GLCPP_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# ────────────────────────────────────────────────────────────────
# glcpp-parse.c / glcpp-parse.h -- meson.build lines 4–22 (Bison).
# -p glcpp_parser_ setzt den Symbol-Praefix (analog zu -p _mesa_glsl_
# in compiler/glsl/CMakeLists.txt), damit dieser Parser nicht mit
# glsl_parser oder anderen Bison-Parsern im selben Binary kollidiert.
# Feste Bison-Syntax (--defines=), siehe Begruendung in
# compiler/glsl/CMakeLists.txt.
# ────────────────────────────────────────────────────────────────
find_program(BISON_EXECUTABLE bison REQUIRED)

set(GLCPP_PARSE_C ${GLCPP_GEN_DIR}/glcpp-parse.c)
set(GLCPP_PARSE_H ${GLCPP_GEN_DIR}/glcpp-parse.h)
add_custom_command(
        OUTPUT ${GLCPP_PARSE_C} ${GLCPP_PARSE_H}
        COMMAND ${BISON_EXECUTABLE} -o ${GLCPP_PARSE_C} -p glcpp_parser_ --defines=${GLCPP_PARSE_H} ${GLCPP_ROOT}/glcpp-parse.y
        DEPENDS ${GLCPP_ROOT}/glcpp-parse.y
        COMMENT "Generating compiler/glsl/glcpp/glcpp-parse.c + .h (bison)"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# glcpp-lex.c -- meson.build lines 24–29 (Flex, reiner C-Modus --
# kein --c++ hier, anders als glsl_lexer.ll in compiler/glsl/).
# ────────────────────────────────────────────────────────────────
find_program(FLEX_EXECUTABLE flex REQUIRED)

set(GLCPP_LEX_C ${GLCPP_GEN_DIR}/glcpp-lex.c)
add_custom_command(
        OUTPUT ${GLCPP_LEX_C}
        COMMAND ${FLEX_EXECUTABLE} -o ${GLCPP_LEX_C} ${GLCPP_ROOT}/glcpp-lex.l
        DEPENDS ${GLCPP_ROOT}/glcpp-lex.l
        COMMENT "Generating compiler/glsl/glcpp/glcpp-lex.c (flex)"
        VERBATIM
)

# glcpp_header_gen_deps (meson.build lines 31–35) ist bei Meson nur ein
# Dependency-Wrapper um glcpp-parse.h, damit Consumer von libglcpp den
# generierten Header als Build-Order-Dependency bekommen, ohne die volle
# Source-Liste zu kennen. In CMake erledigt PUBLIC target_include_directories
# + der generierte Header als Source in add_library() das automatisch --
# kein eigenes Target noetig.

# ────────────────────────────────────────────────────────────────
# libglcpp -- meson.build lines 37–46 (static_library, build_by_default:
# false -> EXCLUDE_FROM_ALL, analog zu mesa_glsl/mesa_vtn).
# ────────────────────────────────────────────────────────────────
add_library(mesa_glcpp STATIC EXCLUDE_FROM_ALL
        ${GLCPP_ROOT}/pp.c
        ${GLCPP_ROOT}/glcpp.h
        ${GLCPP_LEX_C}
        ${GLCPP_PARSE_C}
        ${GLCPP_PARSE_H}
)

target_include_directories(mesa_glcpp
        PUBLIC
        ${GLCPP_ROOT}
        ${GLCPP_GEN_DIR}            # fuer #include "glcpp-parse.h"
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/mesa
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/src/gallium/auxiliary
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_glcpp PRIVATE
        ${MESA_COMMON_C_FLAGS}
)

set_target_properties(mesa_glcpp PROPERTIES
        C_VISIBILITY_PRESET hidden
)

target_link_libraries(mesa_glcpp PUBLIC mesa_util)
add_dependencies(mesa_glcpp sysroot_headers mesa_util)

# ────────────────────────────────────────────────────────────────
# libglcpp_standalone -- meson.build lines 48–58 (static_library,
# build_by_default: false -> EXCLUDE_FROM_ALL). Wird von
# compiler/glsl/CMakeLists.txt fuer mesa_glsl_standalone gebraucht,
# falls ihr das spaeter aktiviert (siehe dortiger auskommentierter
# Block am Dateiende).
# ────────────────────────────────────────────────────────────────
add_library(mesa_glcpp_standalone STATIC EXCLUDE_FROM_ALL
        ${GLCPP_ROOT}/pp_standalone_scaffolding.c
)

target_include_directories(mesa_glcpp_standalone
        PRIVATE
        ${GLCPP_ROOT}
        ${GLCPP_GEN_DIR}
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/mesa
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/src/gallium/auxiliary
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_glcpp_standalone PRIVATE
        ${MESA_COMMON_C_FLAGS}
)

set_target_properties(mesa_glcpp_standalone PROPERTIES
        C_VISIBILITY_PRESET hidden
)

target_link_libraries(mesa_glcpp_standalone PUBLIC mesa_glcpp mesa_util)
add_dependencies(mesa_glcpp_standalone sysroot_headers mesa_glcpp mesa_util)