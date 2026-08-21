# ════════════════════════════════════════════════════════════════
# lib/mesa/src/intel/blorp/CMakeLists.txt
#
# Modeled on upstream src/intel/blorp/meson.build.
#
# ACHTUNG — HOST-TOOL-ABHÄNGIGKEIT, NOCH NICHT AUFGELÖST:
# Dieser Layer braucht zwei Programme, die zur BUILD-Zeit auf dem
# Host laufen (nicht Teil von VesperaOS, nicht mit dem
# x86_64-vespera-elf-Cross-Compiler gebaut):
#
#   MESA_CLC_EXECUTABLE      — mesa_clc, OpenCL-C -> SPIR-V Compiler
#   VTN_BINDGEN2_EXECUTABLE  — vtn_bindgen2, SPIR-V -> eingebetteter
#                              C++-Code-Generator (braucht SPIR-V/NIR-
#                              Parsing, d.h. compiler/spirv als
#                              Host-Build)
#
# Diese beiden Variablen sind hier bewusst als Platzhalter referenziert
# (siehe HOST TOOLS Block unten) — die CMakeLists.txt ist strukturell
# fertig, aber der Build wird fehlschlagen, bis diese zwei Executables
# tatsächlich existieren (separater Host-Build-Baum, z.B. via
# ExternalProject oder ein zweites, natives CMake-Build-Verzeichnis für
# compiler/clc + compiler/spirv + compiler/nir + compiler/libcl).
# Das ist der nächste Schritt nach diesem Layer.
#
# with_intel_elk (blorp_elk-Variante) bewusst ausgelassen — konsistent
# zur Entscheidung im compiler-Layer, kein Gen4-8-Support.
# ════════════════════════════════════════════════════════════════

set(INTEL_BLORP_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(INTEL_BLORP_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# ────────────────────────────────────────────────────────────────
# HOST TOOLS — Platzhalter, siehe Header-Kommentar. Diese Variablen
# müssen VOR add_subdirectory(blorp) im Root-CMakeLists gesetzt sein,
# sobald der Host-Tool-Build existiert (analog zu MESA_PYTHON3_EXECUTABLE
# in FindMesaPython.cmake).
# ────────────────────────────────────────────────────────────────
if(NOT DEFINED MESA_CLC_EXECUTABLE)
    message(WARNING
            "MESA_CLC_EXECUTABLE ist nicht gesetzt — intel/blorp braucht "
            "einen für den Host gebauten mesa_clc-Compiler (OpenCL-C -> "
            "SPIR-V), der noch nicht existiert. Der Codegen-Schritt für "
            "blorp_shaders.spv wird fehlschlagen, bis das aufgelöst ist."
    )
    set(MESA_CLC_EXECUTABLE "MESA_CLC_EXECUTABLE-NOT-SET")
endif()
if(NOT DEFINED VTN_BINDGEN2_EXECUTABLE)
    message(WARNING
            "VTN_BINDGEN2_EXECUTABLE ist nicht gesetzt — intel/blorp braucht "
            "vtn_bindgen2 (SPIR-V -> eingebetteter C++-Code), noch nicht "
            "vorhanden. Betrifft blorp_shaders.cpp/.h-Codegen."
    )
    set(VTN_BINDGEN2_EXECUTABLE "VTN_BINDGEN2_EXECUTABLE-NOT-SET")
endif()

# ────────────────────────────────────────────────────────────────
# blorp_shaders.spv — meson.build lines 8-22. mesa_clc kompiliert die
# .cl-Shader zu SPIR-V.
# ────────────────────────────────────────────────────────────────
set(BLORP_SHADERS_CL ${INTEL_BLORP_ROOT}/blorp_shaders.cl)
set(BLORP_SHADERS_SPV ${INTEL_BLORP_GEN_DIR}/blorp_shaders.spv)
add_custom_command(
        OUTPUT ${BLORP_SHADERS_SPV}
        COMMAND ${MESA_CLC_EXECUTABLE} -o ${BLORP_SHADERS_SPV} --depfile ${INTEL_BLORP_GEN_DIR}/blorp_shaders.h.d
        ${BLORP_SHADERS_CL} --
        -cl-std=cl2.0
        -D__OPENCL_VERSION__=200
        -DHAVE___BUILTIN_FFS
        -DHAVE___BUILTIN_CLZ
        -Wno-initializer-overrides
        -DGFX_VERx10=${gfx_verx10}
        -I${MESA_ROOT}/include
        -I${MESA_ROOT}/src/compiler/libcl
        -I${MESA_ROOT}/src
        -I${MESA_ROOT}/src/intel/compiler
        -I${INTEL_BLORP_ROOT}
        DEPENDS ${BLORP_SHADERS_CL} ${MESA_CLC_EXECUTABLE}
        COMMENT "Generating intel/blorp/blorp_shaders.spv (mesa_clc)"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# blorp_shaders.cpp/.h — meson.build lines 24-28. vtn_bindgen2
# übersetzt das SPIR-V in eingebetteten C++-Code.
# ────────────────────────────────────────────────────────────────
set(BLORP_SHADERS_CPP ${INTEL_BLORP_GEN_DIR}/blorp_shaders.cpp)
set(BLORP_SHADERS_H ${INTEL_BLORP_GEN_DIR}/blorp_shaders.h)
add_custom_command(
        OUTPUT ${BLORP_SHADERS_CPP} ${BLORP_SHADERS_H}
        COMMAND ${VTN_BINDGEN2_EXECUTABLE} ${BLORP_SHADERS_SPV} ${BLORP_SHADERS_CPP} ${BLORP_SHADERS_H}
        DEPENDS ${BLORP_SHADERS_SPV} ${VTN_BINDGEN2_EXECUTABLE}
        COMMENT "Generating intel/blorp/blorp_shaders.[cpp|h] (vtn_bindgen2)"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# libblorp — meson.build lines 35-66.
# ────────────────────────────────────────────────────────────────
set(BLORP_CORE_SOURCES
        blorp.c
        blorp_blit.c
        blorp_clear.c
        blorp_indirect_copy.c
)
set(BLORP_BRW_SOURCES
        blorp_brw.c
)
list(TRANSFORM BLORP_CORE_SOURCES PREPEND "${INTEL_BLORP_ROOT}/")
list(TRANSFORM BLORP_BRW_SOURCES PREPEND "${INTEL_BLORP_ROOT}/")

add_library(mesa_intel_blorp STATIC
        ${BLORP_CORE_SOURCES}
        ${BLORP_BRW_SOURCES}
        ${BLORP_SHADERS_CPP}
        ${BLORP_SHADERS_H}
)

target_include_directories(mesa_intel_blorp
        PUBLIC
        ${INTEL_BLORP_ROOT}
        ${MESA_ROOT}/src
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src/intel/compiler
        ${INTEL_BLORP_ROOT}/..
        ${INTEL_BLORP_GEN_DIR}
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_intel_blorp PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_C_FLAGS}>
)

target_compile_options(mesa_intel_blorp PRIVATE -fvisibility=hidden)

target_link_libraries(mesa_intel_blorp PUBLIC
        mesa_nir
        mesa_util
        mesa_intel_dev
        mesa_intel_compiler_gen
)

add_dependencies(mesa_intel_blorp sysroot_headers mesa_nir mesa_genxml mesa_util mesa_intel_dev mesa_intel_compiler_gen)