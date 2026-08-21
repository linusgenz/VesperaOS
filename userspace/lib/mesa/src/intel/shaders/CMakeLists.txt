# ════════════════════════════════════════════════════════════════
# lib/mesa/src/intel/shaders/CMakeLists.txt
#
# Modeled on upstream src/intel/shaders/meson.build.
#
# ACHTUNG — HOST-TOOL-ABHÄNGIGKEIT, NOCH NICHT AUFGELÖST: gleiche
# Situation wie intel/blorp/CMakeLists.txt (siehe dortigen
# Header-Kommentar für die vollständige Erklärung). Dieser Layer
# braucht MESA_CLC_EXECUTABLE + VTN_BINDGEN2_EXECUTABLE, hier sogar
# mehrfach (einmal pro unterstützter Gen-Generation, siehe unten).
#
# with_intel_vk (Vulkan-spezifische Shader-Varianten, d.h. das
# float64_spv.h custom_target in meson.build) bewusst ausgelassen —
# kein Vulkan-Treiber geplant, nur GL/GLES über iris.
# ════════════════════════════════════════════════════════════════

set(INTEL_SHADERS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(INTEL_SHADERS_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# ────────────────────────────────────────────────────────────────
# HOST TOOLS — siehe intel/blorp/CMakeLists.txt für Erklärung. Selbe
# Variablen, hier erneut geprüft (falls dieser Layer eigenständig vor
# blorp konfiguriert wird), aber nicht doppelt mit WARNING beschwert,
# wenn schon anderswo gesetzt.
# ────────────────────────────────────────────────────────────────
if(NOT DEFINED MESA_CLC_EXECUTABLE)
    set(MESA_CLC_EXECUTABLE "MESA_CLC_EXECUTABLE-NOT-SET")
endif()
if(NOT DEFINED VTN_BINDGEN2_EXECUTABLE)
    set(VTN_BINDGEN2_EXECUTABLE "VTN_BINDGEN2_EXECUTABLE-NOT-SET")
endif()

# ────────────────────────────────────────────────────────────────
# TODO(vespera): genxml-Generierung. In meson.build ist
# gen_cl_xml_pack eine `depends`-Abhängigkeit des CL→SPV-Schritts
# (generiert die genxml-Header, die die .cl-Dateien via -I...genxml
# includieren). Bei uns liefert dafür das Target `mesa_genxml`
# (Verzeichnis src/intel/genxml). Als Build-Order-Dependency unten
# über add_dependencies eingehängt — falls mesa_genxml kein reines
# Custom-Target ist, ggf. anpassen.
# ────────────────────────────────────────────────────────────────

# ────────────────────────────────────────────────────────────────
# TODO(vespera): device-workaround-Quellen. In meson.build ist
# intel_dev_wa_src eine `depends`-Abhängigkeit des CL→SPV-Schritts.
# Bei uns liefert dafür das Target `mesa_intel_dev`
# (Verzeichnis src/intel/dev). Ebenfalls unten als Build-Order-
# Dependency eingehängt.
# ────────────────────────────────────────────────────────────────

# ────────────────────────────────────────────────────────────────
# Quelldateien — meson.build kompiliert NICHT eine .cl-Datei pro Gen,
# sondern dieselbe feste Liste gemeinsamer .cl-Dateien einmal PRO
# Gen-Variante (Unterscheidung erfolgt über -DGFX_VERx10=... beim
# Aufruf von mesa_clc, nicht über unterschiedliche Quelldateien).
# ────────────────────────────────────────────────────────────────
set(INTEL_SHADER_SOURCES
        ${INTEL_SHADERS_ROOT}/libintel_shaders.h
        ${INTEL_SHADERS_ROOT}/dgc.cl
        ${INTEL_SHADERS_ROOT}/dgc_debug.cl
        ${INTEL_SHADERS_ROOT}/generate.cl
        ${INTEL_SHADERS_ROOT}/generate_draws.cl
        ${INTEL_SHADERS_ROOT}/generate_draws_iris.cl
        ${INTEL_SHADERS_ROOT}/query_copy.cl
        ${INTEL_SHADERS_ROOT}/util.cl
)

# ────────────────────────────────────────────────────────────────
# Gen-Varianten — meson.build: intel_shaders_gens ist eine Liste von
# [GFX_VERx10, subdir_suffix]-Paaren. subdir_suffix wird im Upstream
# aktuell nicht für die Dateinamen verwendet (nur GFX_VERx10 fließt
# in 'intel_gfx@0@_shaders.spv'.format(gen[0]) ein), daher genügt
# hier eine einfache Liste der GFX_VERx10-Werte. Vollständige Liste
# aus meson.build übernommen — 80, 90, 110, 120, 125, 200, 300, 350.
# ────────────────────────────────────────────────────────────────
set(INTEL_SHADER_GFX_VERX10_LIST
        80 90 110 120 125 200 300 350
)

set(INTEL_SHADERS_ALL_CPP "")
set(INTEL_SHADERS_ALL_H "")

foreach(gfx_verx10 ${INTEL_SHADER_GFX_VERX10_LIST})
    set(shader_spv ${INTEL_SHADERS_GEN_DIR}/intel_gfx${gfx_verx10}_shaders.spv)
    add_custom_command(
            OUTPUT ${shader_spv}
            COMMAND ${MESA_CLC_EXECUTABLE}
            ${INTEL_SHADER_SOURCES} -o ${shader_spv}
            --depfile ${INTEL_SHADERS_GEN_DIR}/intel_gfx${gfx_verx10}_shaders.spv.d
            --
            -DUTIL_ARCH_LITTLE_ENDIAN=1
            -DUTIL_ARCH_BIG_ENDIAN=0
            -cl-std=cl2.0
            -D__OPENCL_VERSION__=200
            -DHAVE___BUILTIN_FFS
            -DHAVE___BUILTIN_CLZ
            -Wno-initializer-overrides
            -DGFX_VERx10=${gfx_verx10}
            -I${INTEL_SHADERS_ROOT}
            -I${MESA_ROOT}/src/compiler/libcl
            -I${MESA_ROOT}/src
            -I${MESA_ROOT}/src/intel
            -I${CMAKE_CURRENT_BINARY_DIR}/.. # we want to get the source intel dir for the non generated headers from genxml
            -I${MESA_ROOT}/src/intel/genxml

            DEPENDS ${INTEL_SHADER_SOURCES} ${MESA_CLC_EXECUTABLE}
            COMMENT "Generating intel/shaders/intel_gfx${gfx_verx10}_shaders.spv (mesa_clc)"
            VERBATIM
    )

    set(shader_cpp ${INTEL_SHADERS_GEN_DIR}/intel_gfx${gfx_verx10}_shaders_binding.cpp)
    set(shader_h ${INTEL_SHADERS_GEN_DIR}/intel_gfx${gfx_verx10}_shaders_binding.h)
    add_custom_command(
            OUTPUT ${shader_cpp} ${shader_h}
            COMMAND ${VTN_BINDGEN2_EXECUTABLE} ${shader_spv} ${shader_cpp} ${shader_h}
            DEPENDS ${shader_spv} ${VTN_BINDGEN2_EXECUTABLE}
            COMMENT "Generating intel/shaders/intel_gfx${gfx_verx10}_shaders_binding.[cpp|h] (vtn_bindgen2)"
            VERBATIM
    )

    list(APPEND INTEL_SHADERS_ALL_CPP ${shader_cpp})
    list(APPEND INTEL_SHADERS_ALL_H ${shader_h})
endforeach()

# ────────────────────────────────────────────────────────────────
# libintel_shaders — meson.build: static_library('intel_shaders', ...)
# nur die generierten .cpp/.h, keine eigene Wrapper-.c-Datei (die gab
# es in der vorherigen Fassung fälschlich als intel_gpu_shaders.c).
# ────────────────────────────────────────────────────────────────
add_library(mesa_intel_shaders STATIC
        ${INTEL_SHADERS_ALL_CPP}
        ${INTEL_SHADERS_ALL_H}
)

target_include_directories(mesa_intel_shaders
        PUBLIC
        ${INTEL_SHADERS_ROOT}
        ${INTEL_SHADERS_GEN_DIR}
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_intel_shaders PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_C_FLAGS}>
)

target_compile_options(mesa_intel_shaders PRIVATE -fvisibility=hidden)

target_link_libraries(mesa_intel_shaders PUBLIC
        mesa_nir
)

add_dependencies(mesa_intel_shaders sysroot_headers mesa_nir mesa_genxml mesa_util mesa_intel_dev)