# ════════════════════════════════════════════════════════════════
# lib/mesa/src/compiler/spirv/CMakeLists.txt
#
# 1:1-Uebersetzung von upstream src/compiler/spirv/meson.build,
# reduziert analog zum Rest des Baums (siehe compiler/CMakeLists.txt
# Kopfkommentar): keine dep_spirv_tools-Anbindung, kein spirv2nir-
# Standalone-Tool, kein vtn_bindgen2 (host-tools uebernimmt das
# separat ueber ExternalProject_Add, siehe lib/mesa/CMakeLists.txt),
# keine Tests. Gebaut wird nur libvtn selbst, das iris/NIR fuer
# gl_spirv.c / spirv_to_nir.c braucht.
#
# Eingebunden von: lib/mesa/src/compiler/CMakeLists.txt muesste
# add_subdirectory(spirv) NACH add_subdirectory(nir) aufrufen (idep_nir
# muss existieren, bevor mesa_vtn gegen mesa_nir linkt).
# ════════════════════════════════════════════════════════════════

set(SPIRV_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(SPIRV_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# ────────────────────────────────────────────────────────────────
# vtn_gather_types.c — meson.build lines 4–9.
# ────────────────────────────────────────────────────────────────
set(VTN_GATHER_TYPES_C ${SPIRV_GEN_DIR}/vtn_gather_types.c)
add_custom_command(
        OUTPUT ${VTN_GATHER_TYPES_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${SPIRV_ROOT}/vtn_gather_types_c.py ${SPIRV_ROOT}/spirv.core.grammar.json ${VTN_GATHER_TYPES_C}
        DEPENDS ${SPIRV_ROOT}/vtn_gather_types_c.py ${SPIRV_ROOT}/spirv.core.grammar.json
        COMMENT "Generating compiler/spirv/vtn_gather_types.c"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# spirv_info.h / spirv_info.c — meson.build lines 11–18. Ein
# custom_target mit zwei Outputs (--out-h / --out-c), daher hier ein
# einzelner add_custom_command mit beiden OUTPUT-Dateien statt zwei
# getrennter Aufrufe (verhindert, dass das Skript zweimal laeuft).
# ────────────────────────────────────────────────────────────────
set(SPIRV_INFO_H ${SPIRV_GEN_DIR}/spirv_info.h)
set(SPIRV_INFO_C ${SPIRV_GEN_DIR}/spirv_info.c)
add_custom_command(
        OUTPUT ${SPIRV_INFO_H} ${SPIRV_INFO_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${SPIRV_ROOT}/spirv_info_gen.py --json ${SPIRV_ROOT}/spirv.core.grammar.json
        --out-h ${SPIRV_INFO_H} --out-c ${SPIRV_INFO_C}
        DEPENDS ${SPIRV_ROOT}/spirv_info_gen.py ${SPIRV_ROOT}/spirv.core.grammar.json
        COMMENT "Generating compiler/spirv/spirv_info.h + spirv_info.c"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# vtn_generator_ids.h — meson.build lines 20–25.
# ────────────────────────────────────────────────────────────────
set(VTN_GENERATOR_IDS_H ${SPIRV_GEN_DIR}/vtn_generator_ids.h)
add_custom_command(
        OUTPUT ${VTN_GENERATOR_IDS_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${SPIRV_ROOT}/vtn_generator_ids_h.py ${SPIRV_ROOT}/spir-v.xml ${VTN_GENERATOR_IDS_H}
        DEPENDS ${SPIRV_ROOT}/vtn_generator_ids_h.py ${SPIRV_ROOT}/spir-v.xml
        COMMENT "Generating compiler/spirv/vtn_generator_ids.h"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# files_libvtn — meson.build lines 27–46. Fixe Liste, kein Glob.
# ────────────────────────────────────────────────────────────────
set(SPIRV_CORE_SOURCES
        ${SPIRV_ROOT}/gl_spirv.c
        ${SPIRV_ROOT}/spirv_to_nir.c
        ${SPIRV_ROOT}/vtn_alu.c
        ${SPIRV_ROOT}/vtn_amd.c
        ${SPIRV_ROOT}/vtn_cfg.c
        ${SPIRV_ROOT}/vtn_cmat.c
        ${SPIRV_ROOT}/vtn_debug.c
        ${SPIRV_ROOT}/vtn_glsl450.c
        ${SPIRV_ROOT}/vtn_opencl.c
        ${SPIRV_ROOT}/vtn_structured_cfg.c
        ${SPIRV_ROOT}/vtn_subgroup.c
        ${SPIRV_ROOT}/vtn_variables.c
)

# ────────────────────────────────────────────────────────────────
# libvtn — meson.build lines 48–60 (static_library, build_by_default:
# false -> EXCLUDE_FROM_ALL, analog zu glapi_bridge in
# src/mesa/glapi/glapi/CMakeLists.txt).
# ────────────────────────────────────────────────────────────────
add_library(mesa_vtn STATIC
        ${SPIRV_CORE_SOURCES}
        ${SPIRV_INFO_C}
        ${SPIRV_INFO_H}
        ${VTN_GATHER_TYPES_C}
        ${VTN_GENERATOR_IDS_H}
)

target_include_directories(mesa_vtn
        PUBLIC
        ${SPIRV_ROOT}
        ${SPIRV_GEN_DIR}            # fuer #include "spirv_info.h" / "vtn_generator_ids.h"
        ${MESA_ROOT}/include        # inc_include Aequivalent
        ${MESA_ROOT}/src            # inc_src Aequivalent
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_vtn PRIVATE
        ${MESA_COMMON_C_FLAGS}
)

target_link_libraries(mesa_vtn PUBLIC mesa_nir mesa_util)

add_dependencies(mesa_vtn sysroot_headers mesa_nir mesa_util)