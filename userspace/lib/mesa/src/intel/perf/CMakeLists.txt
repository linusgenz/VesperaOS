# ════════════════════════════════════════════════════════════════
# lib/mesa/src/intel/perf/CMakeLists.txt
#
# Modeled on upstream src/intel/perf/meson.build.
#
# Bewusst ausgelassen: i915/intel_perf.c + xe/intel_perf.c (analog zur
# Entscheidung in intel/dev — es gibt nur den Lucifer-KMD-Typ, i915/xe
# Perf-Pfade werden nie erreicht). intel_perf_query_layout (with_intel_tools
# executable) — Kommandozeilen-Tool, kein Bibliotheks-Bestandteil.
#
# HW-Metrics-XML-Codegen (gen_perf.py -> intel_perf_metrics.c/.h) wird
# unverändert übernommen — reiner Python-Codegen wie bei den bisherigen
# Layern, keine Host-Tool-Abhängigkeit.
# ════════════════════════════════════════════════════════════════

set(INTEL_PERF_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(INTEL_PERF_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# ────────────────────────────────────────────────────────────────
# intel_perf_metrics.c/.h — meson.build lines 18-25. gen_perf.py liest
# die kompletten oa-<gen>.xml-Dateien (eine pro HW-Generation) ein und
# erzeugt daraus die Metrics-Tabellen. Volle Gen-Liste übernommen,
# nicht auf eine Generation beschränkt.
# ────────────────────────────────────────────────────────────────
set(INTEL_HW_METRICS
        hsw
        bdw chv
        sklgt2 sklgt3 sklgt4
        kblgt2 kblgt3
        cflgt2 cflgt3
        bxt glk
        icl ehl
        tglgt1 tglgt2 rkl dg1 adl
        acmgt1 acmgt2 acmgt3
        mtlgt2 mtlgt3
        arlgt1 arlgt2
        lnl
        bmg
        ptl
)

set(INTEL_HW_METRICS_XML_FILES "")
foreach(hw ${INTEL_HW_METRICS})
    list(APPEND INTEL_HW_METRICS_XML_FILES ${INTEL_PERF_ROOT}/oa-${hw}.xml)
endforeach()

set(INTEL_PERF_METRICS_C ${INTEL_PERF_GEN_DIR}/intel_perf_metrics.c)
set(INTEL_PERF_METRICS_H ${INTEL_PERF_GEN_DIR}/intel_perf_metrics.h)
add_custom_command(
        OUTPUT ${INTEL_PERF_METRICS_C} ${INTEL_PERF_METRICS_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${INTEL_PERF_ROOT}/gen_perf.py
        --code ${INTEL_PERF_METRICS_C} --header ${INTEL_PERF_METRICS_H}
        ${INTEL_HW_METRICS_XML_FILES}
        DEPENDS ${INTEL_PERF_ROOT}/gen_perf.py ${INTEL_HW_METRICS_XML_FILES}
        COMMENT "Generating intel/perf/intel_perf_metrics.[ch]"
        VERBATIM
)

# ────────────────────────────────────────────────────────────────
# files — meson.build lines 1-16, minus i915/xe (siehe Header-Kommentar).
# ────────────────────────────────────────────────────────────────
set(INTEL_PERF_CORE_SOURCES
        intel_perf_common.c
        intel_perf.c
        intel_perf_query.c
        intel_perf_mdapi.c
        intel_perf_metrics_library.cpp

        i915/intel_perf.c
        xe/intel_perf.c
)
list(TRANSFORM INTEL_PERF_CORE_SOURCES PREPEND "${INTEL_PERF_ROOT}/")

add_library(mesa_intel_perf STATIC
        ${INTEL_PERF_CORE_SOURCES}
        ${INTEL_PERF_METRICS_C}
        ${INTEL_PERF_METRICS_H}
)

target_include_directories(mesa_intel_perf
        PUBLIC
        ${INTEL_PERF_ROOT}
        ${MESA_ROOT}/src
        ${MESA_ROOT}/include
        ${INTEL_PERF_GEN_DIR}
        ${INTEL_PERF_GEN_DIR}/../..
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_intel_perf PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_C_FLAGS}>
)

target_compile_options(mesa_intel_perf PRIVATE -fvisibility=hidden ${MESA_SSE2_ARGS})

target_link_libraries(mesa_intel_perf PUBLIC mesa_util mesa_intel_dev mesa_intel_common mesa_intel_ds)

add_dependencies(mesa_intel_perf sysroot_headers mesa_util mesa_intel_dev mesa_intel_common)