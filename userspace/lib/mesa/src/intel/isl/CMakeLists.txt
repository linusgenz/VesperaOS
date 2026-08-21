# lib/mesa/src/intel/isl/CMakeLists.txt
#
# Portierung von src/intel/isl/meson.build. ISL = Image/Surface Layout --
# beschreibt Tiling, Formate, Aux-Compression etc. fuer Intel-GPU-Surfaces.
#
# Reduziert ggue. upstream auf Gen9 (GFX_VERx10=90) statt der vollen Palette
# (gfx4..gfx350). Upstream baut isl_per_hw_verNN als eigene kleine statische
# Lib PRO Hardware-Generation und linkt alle zusammen in libisl -- das ist
# noetig wenn ein einziges Mesa-Binary alle Generationen zur Laufzeit
# unterstuetzen soll (Gen-Erkennung passiert dynamisch ueber PCI-ID). Da wir
# nur Gen9 als Ziel haben, entfaellt der Grund fuer die Aufspaltung; wir
# kompilieren isl_gfx9_files direkt mit in die Haupt-Lib statt ein
# eigenes Sub-Target zu bauen. Falls ihr spaeter weitere Gens braucht,
# ist das Muster hier (siehe ISL_PER_HW_VER_SOURCES) leicht erweiterbar.
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
# isl_per_hw_ver90 -- Gen9-spezifische Surface-State-Emission.
# isl_per_hw_ver_files (isl_emit_cpb.c, isl_emit_depth_stencil.c,
# isl_surface_state.c) werden PRO Gen-Version mit -DGFX_VERx10=90 neu
# kompiliert -- das ist kein Bug/keine Dopplung, das ist upstream's
# Mechanismus um denselben Quellcode Gen-generisch zu halten (per #if
# GFX_VERx10 >= ... in den .c-Dateien) und trotzdem fuer jede Generation
# eigene, spezialisierte Objektdateien zu erzeugen.
# ---------------------------------------------------------------------------
set(ISL_PER_HW_VER90_SOURCES
        isl_emit_cpb.c
        isl_emit_depth_stencil.c
        isl_surface_state.c
        isl_gfx9.c
)

add_library(mesa_isl_per_hw_ver90 STATIC ${ISL_PER_HW_VER90_SOURCES})

target_include_directories(mesa_isl_per_hw_ver90
        PUBLIC
        ${ISL_DIR}
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/intel
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_definitions(mesa_isl_per_hw_ver90 PRIVATE GFX_VERx10=90)

target_link_libraries(mesa_isl_per_hw_ver90
        PUBLIC
        mesa_util
        mesa_intel_dev
)

target_compile_options(mesa_isl_per_hw_ver90 PRIVATE ${MESA_COMMON_C_FLAGS})

add_dependencies(mesa_isl_per_hw_ver90 sysroot_headers mesa_genxml)
target_include_directories(mesa_isl_per_hw_ver90 PRIVATE ${GENXML_INCLUDE_DIRS})

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

add_library(mesa_isl STATIC ${MESA_ISL_SOURCES})

target_include_directories(mesa_isl
        PUBLIC
        ${ISL_DIR}
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/intel
        ${GENXML_INCLUDE_DIRS}
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_link_libraries(mesa_isl
        PUBLIC
        mesa_util
        mesa_intel_dev
        mesa_isl_per_hw_ver90
        mesa_isl_tiled_memcpy
)

target_compile_options(mesa_isl PRIVATE ${MESA_COMMON_C_FLAGS})

add_dependencies(mesa_isl sysroot_headers mesa_genxml)

install(TARGETS mesa_isl ARCHIVE DESTINATION ${VESPERA_SYSROOT}/usr/lib COMPONENT runtime)