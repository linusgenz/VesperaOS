# lib/mesa/src/intel/isl/CMakeLists.txt
#
# Portierung von src/intel/isl/meson.build. ISL = Image/Surface Layout --
# beschreibt Tiling, Formate, Aux-Compression etc. fuer Intel-GPU-Surfaces.
#
# Baut alle Gen-Versionen (isl_per_hw_verNN fuer 40..350), 1:1 wie upstream
# via foreach-Schleife in meson.build: pro Hardware-Generation eine eigene
# statische Lib mit -DGFX_VERx10=NN, damit derselbe Quellcode (per #if
# GFX_VERx10 >= ...) gen-spezifisch kompiliert und die Gen-Erkennung zur
# Laufzeit ueber PCI-ID erfolgen kann. Alle Gen-Libs werden in
# mesa_intel_isl gelinkt.
#
# Ausgelassen: with_sse41-Variante (isl_tiled_memcpy_sse41 -- SIMD-optimierter
# Pfad, reine Performance-Optimierung, isl_tiled_memcpy_normal.c reicht
# fuer Korrektheit), with_intel_tools (isl_query), with_tests.

set(ISL_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ISL_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# ---------------------------------------------------------------------------
# isl_format_layout.c -- generiert aus CSV via gen_format_layout.py
# ---------------------------------------------------------------------------
set(ISL_FORMAT_LAYOUT_C ${ISL_GEN_DIR}/isl_format_layout.c)
add_custom_command(
        OUTPUT ${ISL_FORMAT_LAYOUT_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${ISL_DIR}/gen_format_layout.py
        --csv ${ISL_DIR}/isl_format_layout.csv
        --out ${ISL_FORMAT_LAYOUT_C}
        DEPENDS ${ISL_DIR}/gen_format_layout.py ${ISL_DIR}/isl_format_layout.csv
        COMMENT "Generating isl_format_layout.c"
        VERBATIM
)

# ---------------------------------------------------------------------------
# isl_per_hw_verNN -- pro Gen-Version eine eigene statische Lib.
#
# 1:1-Portierung der foreach-Schleife aus meson.build. isl_per_hw_ver_files
# (isl_emit_cpb.c, isl_emit_depth_stencil.c, isl_surface_state.c) werden PRO
# Gen-Version mit -DGFX_VERx10=NN neu kompiliert -- kein Bug/keine Dopplung,
# sondern upstream's Mechanismus, denselben Quellcode Gen-generisch zu
# halten (per #if GFX_VERx10 >= ... in den .c-Dateien) und trotzdem fuer
# jede Generation eigene, spezialisierte Objektdateien zu erzeugen.
#
# Manche Gens (50, 75, 110) haben KEINE eigene isl_gfxN.c -- die nutzen nur
# isl_per_hw_ver_files mit dem jeweiligen GFX_VERx10-Define (siehe leere
# []-Liste in der meson foreach-Schleife). isl_gfx125 und isl_gfx12 teilen
# sich dieselbe isl_gfx12.c/h (siehe meson: beide Eintraege verweisen auf
# dieselben Dateien -- kein Copy-Paste-Fehler in meson.build, sondern der
# Gen12.5-Codepfad hat keine eigenen Aenderungen ggue. Gen12 gebraucht).
# ---------------------------------------------------------------------------
set(ISL_PER_HW_VER_COMMON_SOURCES
        isl_emit_cpb.c
        isl_emit_depth_stencil.c
        isl_surface_state.c
)

# Zwei parallele Listen statt verschachtelter ';'-Strings -- CMake-Listen
# sind selbst schon Semikolon-getrennt, verschachtelte Strings mit ';'
# darin brechen beim foreach(IN LISTS ...), weil CMake sie vorher schon
# expandiert (das war der Bug in der vorigen Version: "120;isl_gfx12.c"
# wurde zu zwei separaten Listeneintraegen statt einem Paar).
set(ISL_GFX_VERS
        40 50 60 70 75 80 90 110 120 125 200 300 350
)
set(ISL_GFX_EXTRA_40   isl_gfx4.c)
set(ISL_GFX_EXTRA_50   "")
set(ISL_GFX_EXTRA_60   isl_gfx6.c)
set(ISL_GFX_EXTRA_70   isl_gfx7.c)
set(ISL_GFX_EXTRA_75   "")
set(ISL_GFX_EXTRA_80   isl_gfx8.c)
set(ISL_GFX_EXTRA_90   isl_gfx9.c)
set(ISL_GFX_EXTRA_110  "")
set(ISL_GFX_EXTRA_120  isl_gfx12.c)
set(ISL_GFX_EXTRA_125  isl_gfx12.c)
set(ISL_GFX_EXTRA_200  isl_gfx20.c)
set(ISL_GFX_EXTRA_300  isl_gfx20.c)
set(ISL_GFX_EXTRA_350  isl_gfx20.c)

set(ISL_PER_HW_VER_LIBS "")

foreach(_gfx_ver IN LISTS ISL_GFX_VERS)
    set(_gfx_extra_c "${ISL_GFX_EXTRA_${_gfx_ver}}")

    set(_gfx_target mesa_isl_per_hw_ver${_gfx_ver})
    set(_gfx_sources ${ISL_PER_HW_VER_COMMON_SOURCES})
    if(_gfx_extra_c)
        list(APPEND _gfx_sources ${_gfx_extra_c})
    endif()

    add_library(${_gfx_target} STATIC ${_gfx_sources})

    target_include_directories(${_gfx_target}
            PUBLIC
            ${ISL_DIR}
            ${MESA_ROOT}/include
            ${MESA_ROOT}/src
            ${MESA_ROOT}/src/intel
            PRIVATE
            ${VESPERA_SYSROOT}/usr/include
    )

    target_compile_definitions(${_gfx_target} PRIVATE GFX_VERx10=${_gfx_ver})

    target_link_libraries(${_gfx_target}
            PUBLIC
            mesa_util
            mesa_intel_dev
    )

    target_compile_options(${_gfx_target} PRIVATE ${MESA_COMMON_C_FLAGS})

    add_dependencies(${_gfx_target} sysroot_headers mesa_genxml)
    target_include_directories(${_gfx_target} PRIVATE ${GENXML_INCLUDE_DIRS})

    set_target_properties(${_gfx_target} PROPERTIES C_VISIBILITY_PRESET hidden)

    list(APPEND ISL_PER_HW_VER_LIBS ${_gfx_target})
endforeach()

# ---------------------------------------------------------------------------
# isl_tiled_memcpy -- Tiled-Memory-Copy-Pfad, ohne SSE4.1-Variante
# ---------------------------------------------------------------------------
add_library(mesa_isl_tiled_memcpy STATIC ${ISL_DIR}/isl_tiled_memcpy_normal.c)

target_include_directories(mesa_isl_tiled_memcpy
        PUBLIC
        ${ISL_DIR}
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/intel
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_link_libraries(mesa_isl_tiled_memcpy
        PUBLIC
        mesa_util
        mesa_intel_dev
)

target_compile_options(mesa_isl_tiled_memcpy PRIVATE ${MESA_COMMON_C_FLAGS})

add_dependencies(mesa_isl_tiled_memcpy sysroot_headers)

# ---------------------------------------------------------------------------
# libisl -- Haupt-Lib
# ---------------------------------------------------------------------------
set(MESA_ISL_SOURCES
        isl.c
        isl_aux_info.c
        isl_drm.c
        isl_format.c
        isl_storage_image.c

        ${ISL_FORMAT_LAYOUT_C}
)

add_library(mesa_intel_isl STATIC ${MESA_ISL_SOURCES})

target_include_directories(mesa_intel_isl
        PUBLIC
        ${ISL_DIR}
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/intel
        ${GENXML_INCLUDE_DIRS}
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_link_libraries(mesa_intel_isl
        PUBLIC
        mesa_util
        mesa_intel_dev
        ${ISL_PER_HW_VER_LIBS}
        mesa_isl_tiled_memcpy
)

target_compile_options(mesa_intel_isl PRIVATE ${MESA_COMMON_C_FLAGS})

add_dependencies(mesa_intel_isl sysroot_headers mesa_genxml)

install(TARGETS mesa_intel_isl ARCHIVE DESTINATION ${VESPERA_SYSROOT}/usr/lib COMPONENT runtime)