# ════════════════════════════════════════════════════════════════
# lib/mesa/src/compiler/glsl/CMakeLists.txt
#
# 1:1-Uebersetzung von upstream src/compiler/glsl/meson.build.
#
# NICHT portiert: tests/ (with_tests), da wir wie ueberall im Baum
# keine gtest-Anbindung haben.
#
# glsl_compiler (Standalone-Tool) wird NICHT gebaut (analog zu
# spirv2nir in compiler/spirv/CMakeLists.txt) -- iris/GL braucht nur
# libglsl selbst, nicht das CLI-Tool. Falls ihr das Tool spaeter zum
# Debuggen von Shadern braucht, ist der Block unten auskommentiert
# und kann reaktiviert werden.
#
# Eingebunden von: lib/mesa/src/compiler/CMakeLists.txt via
# add_subdirectory(glsl), NACH add_subdirectory(nir) (idep_nir wird
# gebraucht) und NACH add_subdirectory(spirv), falls spirv frueher
# im Baum liegt -- Reihenfolge zwischen glsl und spirv ist bei Mesa
# selbst unabhaengig, es gibt keine Kreuzabhaengigkeit.
# ════════════════════════════════════════════════════════════════

set(GLSL_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(GLSL_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# ────────────────────────────────────────────────────────────────
# glsl_util_files -- src/meson.build (nicht compiler/glsl/meson.build!).
# Absichtlich referenzierte Dateien aus src/mesa/, unabhaengig von
# FILES_LIBMESA in src/mesa/CMakeLists.txt -- glsl_util ist ein
# eigenstaendiges kleines Static-Lib fuer Standalone-Tools, das nicht
# gegen die grosse mesa-Lib linken soll. Duplizierte Kompilierung
# derselben .c in zwei Targets ist beabsichtigt (Meson macht das
# genauso: eigene Object-Files pro static_library).
# ────────────────────────────────────────────────────────────────
set(GLSL_UTIL_SOURCES
        ${MESA_ROOT}/src/mesa/main/extensions_table.c
        ${MESA_ROOT}/src/mesa/program/prog_parameter.c
        ${MESA_ROOT}/src/mesa/program/symbol_table.c
        ${MESA_ROOT}/src/mesa/program/dummy_errors.c
)

add_library(mesa_glsl_util STATIC # EXCLUDE_FROM_ALL
        ${GLSL_UTIL_SOURCES}
)

target_include_directories(mesa_glsl_util
        PRIVATE
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/mesa
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/src/gallium/auxiliary
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_glsl_util PRIVATE
        ${MESA_COMMON_C_FLAGS}
)

set_target_properties(mesa_glsl_util PROPERTIES
        C_VISIBILITY_PRESET hidden
)

target_link_libraries(mesa_glsl_util PRIVATE mesa_util)
add_dependencies(mesa_glsl_util sysroot_headers mesa_util)

# ────────────────────────────────────────────────────────────────
# subdir('glcpp') -- meson.build line 13. libglcpp / libglcpp_standalone
# muessen existieren, bevor mesa_glsl dagegen linkt (Zeile weiter
# unten: target_link_libraries(mesa_glsl PRIVATE mesa_glcpp)).
# ────────────────────────────────────────────────────────────────
add_subdirectory(glcpp)

# ────────────────────────────────────────────────────────────────
# glsl_parser.cpp / glsl_parser.h -- meson.build lines 15–33 (Bison).
# Meson unterscheidet bison_is_yacc fuer -H vs --defines=; wir setzen
# fest auf Bison-Syntax (--defines=), da ihr durchgehend Bison nutzt
# (siehe FindMesaPython.cmake / host-tools-Annahmen im Rest des Baums).
# -p _mesa_glsl_ setzt den Symbol-Praefix, damit glsl_parser nicht mit
# anderen Bison-Parsern im selben Binary kollidiert (z.B. Programm-Parser).
# ────────────────────────────────────────────────────────────────
find_program(BISON_EXECUTABLE bison REQUIRED)

set(GLSL_PARSER_CPP ${GLSL_GEN_DIR}/glsl_parser.cpp)
set(GLSL_PARSER_H ${GLSL_GEN_DIR}/glsl_parser.h)
add_custom_command(
        OUTPUT ${GLSL_PARSER_CPP} ${GLSL_PARSER_H}
        COMMAND ${BISON_EXECUTABLE} -o ${GLSL_PARSER_CPP} -p _mesa_glsl_ --defines=${GLSL_PARSER_H} ${GLSL_ROOT}/glsl_parser.yy
        DEPENDS ${GLSL_ROOT}/glsl_parser.yy
        COMMENT "Generating compiler/glsl/glsl_parser.cpp + .h (bison)"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# glsl_lexer.cpp -- meson.build lines 35–40 (Flex, C++-Modus).
# ────────────────────────────────────────────────────────────────
find_program(FLEX_EXECUTABLE flex REQUIRED)

set(GLSL_LEXER_CPP ${GLSL_GEN_DIR}/glsl_lexer.cpp)
add_custom_command(
        OUTPUT ${GLSL_LEXER_CPP}
        COMMAND ${FLEX_EXECUTABLE} -o ${GLSL_LEXER_CPP} ${GLSL_ROOT}/glsl_lexer.ll
        DEPENDS ${GLSL_ROOT}/glsl_lexer.ll
        COMMENT "Generating compiler/glsl/glsl_lexer.cpp (flex)"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# ir_expression_operation_constant.h / _strings.h -- meson.build
# lines 42–56. Gleiches Skript wie ir_expression_operation.h aus
# compiler/CMakeLists.txt (dort Modus 'enum'), hier zwei weitere
# Modi desselben Python-Skripts.
# ────────────────────────────────────────────────────────────────
set(IR_EXPRESSION_OPERATION_CONSTANT_H ${GLSL_GEN_DIR}/ir_expression_operation_constant.h)
add_custom_command(
        OUTPUT ${IR_EXPRESSION_OPERATION_CONSTANT_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GLSL_ROOT}/ir_expression_operation.py constant > ${IR_EXPRESSION_OPERATION_CONSTANT_H}
        DEPENDS ${GLSL_ROOT}/ir_expression_operation.py
        COMMENT "Generating compiler/glsl/ir_expression_operation_constant.h"
        VERBATIM
)

set(IR_EXPRESSION_OPERATION_STRINGS_H ${GLSL_GEN_DIR}/ir_expression_operation_strings.h)
add_custom_command(
        OUTPUT ${IR_EXPRESSION_OPERATION_STRINGS_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GLSL_ROOT}/ir_expression_operation.py strings > ${IR_EXPRESSION_OPERATION_STRINGS_H}
        DEPENDS ${GLSL_ROOT}/ir_expression_operation.py
        COMMENT "Generating compiler/glsl/ir_expression_operation_strings.h"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# xxd-generierte Header -- meson.build lines 58–98. files_xxd liegt
# laut Vorgabe in src/util/. Sechs custom_targets, gleiches Skript,
# unterschiedliches Input-.glsl + Output-Name + -n-Symbolname.
# Kein 'capture: true' bei Meson hier (anders als bei den obigen
# Python-Skripten) -- xxd.py schreibt selbst nach @OUTPUT@, daher
# kein Shell-Redirect '>' im COMMAND.
# ────────────────────────────────────────────────────────────────
set(XXD_PY ${MESA_ROOT}/src/util/xxd.py)

function(glsl_xxd_header out_var input_glsl output_h symbol_name)
    set(_out ${GLSL_GEN_DIR}/${output_h})
    add_custom_command(
            OUTPUT ${_out}
            COMMAND ${MESA_PYTHON3_EXECUTABLE} ${XXD_PY} ${GLSL_ROOT}/${input_glsl} ${_out} -n ${symbol_name}
            DEPENDS ${XXD_PY} ${GLSL_ROOT}/${input_glsl}
            COMMENT "Generating compiler/glsl/${output_h} (xxd)"
            VERBATIM
    )
    set(${out_var} ${_out} PARENT_SCOPE)
endfunction()

glsl_xxd_header(FLOAT64_GLSL_H float64.glsl float64_glsl.h float64_source)
glsl_xxd_header(CROSS_PLATFORM_SETTINGS_PIECE_ALL_H CrossPlatformSettings_piece_all.glsl cross_platform_settings_piece_all.h cross_platform_settings_piece_all_header)
glsl_xxd_header(BC1_GLSL_H bc1.glsl bc1_glsl.h bc1_source)
glsl_xxd_header(BC4_GLSL_H bc4.glsl bc4_glsl.h bc4_source)
glsl_xxd_header(ETC2_RGBA_STITCH_GLSL_H etc2_rgba_stitch.glsl etc2_rgba_stitch_glsl.h etc2_rgba_stitch_source)
glsl_xxd_header(ASTC_GLSL_H astc_decoder.glsl astc_glsl.h astc_source)

# libglsl_headers -- meson.build lines 202–205. IR_EXPRESSION_OPERATION_H
# kommt aus compiler/CMakeLists.txt (PARENT_SCOPE, existiert bereits im
# Scope hier, da compiler/CMakeLists.txt add_subdirectory(nir) und
# add_subdirectory(spirv) VOR add_subdirectory(glsl) aufruft -- siehe
# dortigen Kopfkommentar zur Reihenfolge).
if(NOT DEFINED IR_EXPRESSION_OPERATION_H OR IR_EXPRESSION_OPERATION_H STREQUAL "")
    message(FATAL_ERROR
            "IR_EXPRESSION_OPERATION_H is not set. lib/mesa/src/compiler/CMakeLists.txt "
            "must generate it and add_subdirectory(glsl) must run afterwards."
    )
endif()

set(GLSL_HEADERS
        ${IR_EXPRESSION_OPERATION_H}
        ${CROSS_PLATFORM_SETTINGS_PIECE_ALL_H}
        ${BC1_GLSL_H}
        ${BC4_GLSL_H}
        ${ETC2_RGBA_STITCH_GLSL_H}
        ${ASTC_GLSL_H}
)

# ────────────────────────────────────────────────────────────────
# files_libglsl -- meson.build lines 100–193. Fixe Liste, kein Glob.
# ────────────────────────────────────────────────────────────────
set(GLSL_CORE_SOURCES
        ${GLSL_ROOT}/ast_array_index.cpp
        ${GLSL_ROOT}/ast_expr.cpp
        ${GLSL_ROOT}/ast_function.cpp
        ${GLSL_ROOT}/ast_to_hir.cpp
        ${GLSL_ROOT}/ast_type.cpp
        ${GLSL_ROOT}/builtin_functions.cpp
        ${GLSL_ROOT}/builtin_types.cpp
        ${GLSL_ROOT}/builtin_variables.cpp
        ${GLSL_ROOT}/gl_nir_detect_function_recursion.c
        ${GLSL_ROOT}/gl_nir_lower_atomics.c
        ${GLSL_ROOT}/gl_nir_lower_images.c
        ${GLSL_ROOT}/gl_nir_lower_blend_equation_advanced.c
        ${GLSL_ROOT}/gl_nir_lower_buffers.c
        ${GLSL_ROOT}/gl_nir_lower_discard_flow.c
        ${GLSL_ROOT}/gl_nir_lower_named_interface_blocks.c
        ${GLSL_ROOT}/gl_nir_lower_packed_varyings.c
        ${GLSL_ROOT}/gl_nir_lower_samplers.c
        ${GLSL_ROOT}/gl_nir_lower_samplers_as_deref.c
        ${GLSL_ROOT}/gl_nir_lower_xfb_varying.c
        ${GLSL_ROOT}/gl_nir_link_atomics.c
        ${GLSL_ROOT}/gl_nir_link_functions.c
        ${GLSL_ROOT}/gl_nir_link_interface_blocks.c
        ${GLSL_ROOT}/gl_nir_link_uniform_blocks.c
        ${GLSL_ROOT}/gl_nir_link_uniform_initializers.c
        ${GLSL_ROOT}/gl_nir_link_uniforms.c
        ${GLSL_ROOT}/gl_nir_link_varyings.c
        ${GLSL_ROOT}/gl_nir_link_xfb.c
        ${GLSL_ROOT}/gl_nir_linker.c
        ${GLSL_ROOT}/glsl_parser_extras.cpp
        ${GLSL_ROOT}/glsl_symbol_table.cpp
        ${GLSL_ROOT}/glsl_to_nir.cpp
        ${GLSL_ROOT}/hir_field_selection.cpp
        ${GLSL_ROOT}/ir_basic_block.cpp
        ${GLSL_ROOT}/ir_builder.cpp
        ${GLSL_ROOT}/ir_clone.cpp
        ${GLSL_ROOT}/ir_constant_expression.cpp
        ${GLSL_ROOT}/ir.cpp
        ${GLSL_ROOT}/ir_expression_flattening.cpp
        ${GLSL_ROOT}/ir_function_detect_recursion.cpp
        ${GLSL_ROOT}/ir_function.cpp
        ${GLSL_ROOT}/ir_hierarchical_visitor.cpp
        ${GLSL_ROOT}/ir_hv_accept.cpp
        ${GLSL_ROOT}/ir_print_visitor.cpp
        ${GLSL_ROOT}/ir_rvalue_visitor.cpp
        ${GLSL_ROOT}/ir_validate.cpp
        ${GLSL_ROOT}/ir_variable_refcount.cpp
        ${GLSL_ROOT}/linker_util.cpp
        ${GLSL_ROOT}/lower_builtins.cpp
        ${GLSL_ROOT}/lower_instructions.cpp
        ${GLSL_ROOT}/lower_jumps.cpp
        ${GLSL_ROOT}/lower_mat_op_to_vec.cpp
        ${GLSL_ROOT}/lower_packing_builtins.cpp
        ${GLSL_ROOT}/lower_precision.cpp
        ${GLSL_ROOT}/lower_subroutine.cpp
        ${GLSL_ROOT}/lower_vec_index_to_cond_assign.cpp
        ${GLSL_ROOT}/lower_vector_derefs.cpp
        ${GLSL_ROOT}/opt_algebraic.cpp
        ${GLSL_ROOT}/opt_dead_builtin_variables.cpp
        ${GLSL_ROOT}/opt_dead_code.cpp
        ${GLSL_ROOT}/opt_flatten_nested_if_blocks.cpp
        ${GLSL_ROOT}/opt_function_inlining.cpp
        ${GLSL_ROOT}/opt_if_simplification.cpp
        ${GLSL_ROOT}/opt_minmax.cpp
        ${GLSL_ROOT}/opt_rebalance_tree.cpp
        ${GLSL_ROOT}/opt_tree_grafting.cpp
        ${GLSL_ROOT}/propagate_invariance.cpp
        ${GLSL_ROOT}/string_to_uint_map.cpp
        ${GLSL_ROOT}/serialize.cpp
        ${GLSL_ROOT}/shader_cache.cpp
)

# ────────────────────────────────────────────────────────────────
# libglsl -- meson.build lines 207–219 (static_library, build_by_default:
# false -> EXCLUDE_FROM_ALL, analog zu glapi_bridge / mesa_vtn).
# ────────────────────────────────────────────────────────────────
add_library(mesa_glsl STATIC # EXCLUDE_FROM_ALL
        ${GLSL_CORE_SOURCES}
        ${GLSL_PARSER_CPP}
        ${GLSL_PARSER_H}
        ${GLSL_LEXER_CPP}
        ${GLSL_HEADERS}
        ${IR_EXPRESSION_OPERATION_STRINGS_H}
        ${IR_EXPRESSION_OPERATION_CONSTANT_H}
        ${FLOAT64_GLSL_H}
)

target_include_directories(mesa_glsl
        PUBLIC
        ${GLSL_ROOT}
        ${GLSL_GEN_DIR}             # fuer #include "glsl_parser.h" / "*_glsl.h" / "ir_expression_operation*.h"
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/mesa
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/src/gallium/auxiliary
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_glsl PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_CXX_FLAGS}>
)

set_target_properties(mesa_glsl PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

# link_with : libglcpp -- Name des glcpp-Targets muss mit dem
# add_library()-Aufruf in glcpp/CMakeLists.txt uebereinstimmen.
target_link_libraries(mesa_glsl
        PUBLIC
        mesa_nir
        mesa_util
        PRIVATE
        mesa_glcpp
)

add_dependencies(mesa_glsl sysroot_headers mesa_nir mesa_util mesa_glcpp)

# ────────────────────────────────────────────────────────────────
# glsl_compiler -- meson.build lines 238–249 (Standalone-Tool). NICHT
# gebaut, siehe Kopfkommentar. Block bewusst auskommentiert stehen
# gelassen als Referenz, falls ihr das Tool spaeter braucht -- dann
# zusaetzlich libglsl_standalone (meson.build lines 226–236) aus
# files_libglsl_standalone (standalone.cpp/.h, standalone_scaffolding.cpp/.h)
# portieren, die hier noch fehlt.
# ────────────────────────────────────────────────────────────────
# add_executable(glsl_compiler ${GLSL_ROOT}/main.cpp)
# target_link_libraries(glsl_compiler PRIVATE mesa_glsl_standalone)