# lib/mesa/src/intel/compiler/CMakeLists.txt
#
# Entspricht Mesa upstream src/intel/compiler/meson.build.
#
# Bewusst ausgelassen ggue. upstream:
#   - jay/     : Backend-Compiler fuer Xe2/Xe3-Architekturen (neuer
#                Instruktionssatz als Gen9). Nicht relevant fuer Gen9-only.
#   - elk/     : Alter Backend-Compiler fuer Gen4-8 (Vorgaenger von brw,
#                nur fuer sehr alte Hardware). Gen9 laeuft ueber brw, nicht
#                elk -- daher komplett raus.
#   - Tests, with_intel_tools (gentool) : wie ueberall sonst im Baum.
#
# Reihenfolge: mda (Debug-Archive-Utility, braucht nur mesa_util) -> gen
# (Opcode/Encoding-Codegen) -> intel_compiler_nir (kleine NIR-Lowering-
# Passes) -> brw (der eigentliche Gen9-Backend-Compiler).
#
# idep_mda aufgeloest: src/intel/mda/meson.build, kleine Debug-Archive-Lib.
# Liegt bei uns als eigenstaendiger Layer unter src/intel/mda/, NICHT unter
# src/intel/compiler/mda/ -- daher relativer Pfad ../mda nach oben.

add_subdirectory(../mda ${CMAKE_CURRENT_BINARY_DIR}/mda)
add_subdirectory(gen)

# --- brw_device_sha1_gen.c: Codegen aus intel_device_info.py + eigenem py ---
set(BRW_DEVICE_SHA1_GEN_C ${CMAKE_CURRENT_BINARY_DIR}/brw_device_sha1_gen.c)
add_custom_command(
        OUTPUT ${BRW_DEVICE_SHA1_GEN_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/brw_device_sha1_gen_c.py
        --out ${BRW_DEVICE_SHA1_GEN_C}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/brw_device_sha1_gen_c.py
        ${CMAKE_CURRENT_SOURCE_DIR}/../dev/intel_device_info.py
        COMMENT "Generating brw_device_sha1_gen.c"
        VERBATIM
)
add_custom_target(mesa_intel_brw_device_sha1 DEPENDS ${BRW_DEVICE_SHA1_GEN_C})

# ---------------------------------------------------------------------------
# intel_compiler_nir -- kleine, eigenstaendige NIR-Lowering-Passes,
# intel-spezifisch aber nicht Gen-Backend-spezifisch (brw und jay nutzen
# beide diese Passes, daher als eigenes kleines statisches Lib-Target wie
# in upstream).
# ---------------------------------------------------------------------------
set(MESA_INTEL_COMPILER_NIR_SOURCES
        intel_nir.c

        intel_nir_blockify_uniform_loads.c
        intel_nir_clamp_image_1d_2d_array_sizes.c
        intel_nir_clamp_per_vertex_loads.c
        intel_nir_lower_non_uniform_barycentric_at_sample.c
        intel_nir_lower_non_uniform_resource_intel.c
        intel_nir_lower_printf.c
        intel_nir_lower_shading_rate_output.c
        intel_nir_lower_sparse.c
        intel_nir_opt_peephole_ffma.c
        intel_nir_opt_peephole_imul32x16.c
        intel_nir_tcs_workarounds.c
)

add_library(mesa_intel_compiler_nir STATIC ${MESA_INTEL_COMPILER_NIR_SOURCES})

target_include_directories(mesa_intel_compiler_nir
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_link_libraries(mesa_intel_compiler_nir
        PUBLIC
        mesa_util
        mesa_intel_dev
        mesa_intel_mda
        mesa_nir
        # idep_vtn: SPIR-V -> NIR aus compiler/spirv. Noch nicht eingehaengt
        # (compiler/spirv ist laut Root-CMake noch auskommentiert). Sobald
        # der spirv-Layer steht, hier mesa_spirv (o.ae. Targetname) ergaenzen.
)

target_compile_options(mesa_intel_compiler_nir PRIVATE ${MESA_COMMON_C_FLAGS})

add_dependencies(mesa_intel_compiler_nir sysroot_headers mesa_intel_mda)

# ---------------------------------------------------------------------------
# brw -- der eigentliche Gen9-Backend-Compiler (NIR -> Intel-ISA)
# ---------------------------------------------------------------------------
add_subdirectory(brw)

add_custom_target(mesa_intel_compiler
        DEPENDS
        mesa_genxml
        mesa_intel_mda
        mesa_intel_compiler_gen
        mesa_intel_compiler_nir
        mesa_intel_compiler_brw
        COMMENT "All active intel/compiler layers (mda, gen, nir, brw)"
)