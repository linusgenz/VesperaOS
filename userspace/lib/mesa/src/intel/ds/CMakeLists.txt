# ════════════════════════════════════════════════════════════════
# lib/mesa/src/intel/ds/CMakeLists.txt
#
# Modeled on upstream src/intel/ds/meson.build.
#
# Android-Zweig (with_platform_android / --android an
# intel_tracepoints.py) bewusst ausgelassen — kein Android-Target.
#
# PERFETTO — NOCH NICHT VERDRAHTET:
# VESPERA_WITH_PERFETTO existiert hier nur als Platzhalter-Option.
# dep_perfetto ist bei uns noch nicht als CMake-Target verfuegbar
# (kein find_package/FetchContent dafuer). Wird die Option trotzdem
# eingeschaltet, gibt es nur eine WARNING und der Perfetto-Teil
# bleibt inaktiv — kein FATAL_ERROR, damit der restliche Build nicht
# blockiert wird. intel_tracepoints_perfetto.h wird zwar weiterhin
# vom Python-Codegen mit erzeugt (kostet nichts), aber nirgends
# eingebunden, solange die Option aus ist.
#
# pps_intel (Perfetto Producer Service, Intel-GPU-Performance-
# Counter) bewusst NICHT portiert — haengt zusaetzlich an libdrm,
# das bei uns noch nicht Teil des Sysroots/Cross-Builds ist. Upstream
# meson.build Zeilen 57-83 (pps_intel_lib, pps_intel_dep) sind daher
# hier absichtlich nicht nachgebaut. Nachruesten, sobald sowohl
# Perfetto als auch libdrm im Cross-Build verfuegbar sind.
# ════════════════════════════════════════════════════════════════

set(INTEL_DS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(INTEL_DS_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

option(VESPERA_WITH_PERFETTO "Intel driver debug-stream (ds) mit Perfetto-Tracing bauen" OFF)

if(VESPERA_WITH_PERFETTO)
    message(WARNING
            "VESPERA_WITH_PERFETTO ist eingeschaltet, aber Perfetto ist im "
            "Cross-Build noch nicht als Dependency verdrahtet (kein "
            "find_package/FetchContent fuer dep_perfetto vorhanden). Der "
            "Perfetto-Teil von intel/ds bleibt trotz gesetzter Option "
            "inaktiv, bis das nachgeruestet ist."
    )
endif()

# ────────────────────────────────────────────────────────────────
# intel_tracepoints.[ch] — meson.build lines 10-25. Python-Codegen,
# analog zum genxml-Muster (src/intel/genxml/CMakeLists.txt):
# intel_tracepoints.py erzeugt aus einer Beschreibung drei Dateien.
# --android bewusst weggelassen (kein Android-Target).
#
# TODO(vespera): u_trace_py als DEPENDS ergaenzen, sobald der Pfad
# zu diesem Skript in einer gemeinsamen Variable vorliegt (analog zu
# MESA_PYTHON3_EXECUTABLE aus FindMesaPython.cmake) — aktuell nur
# als Kommentar vermerkt, falls u_trace.py sich aendert und ein
# Rebuild der Tracepoints noetig wird, wird das sonst nicht erkannt.
# ────────────────────────────────────────────────────────────────
set(INTEL_TRACEPOINTS_H ${INTEL_DS_GEN_DIR}/intel_tracepoints.h)
set(INTEL_TRACEPOINTS_PERFETTO_H ${INTEL_DS_GEN_DIR}/intel_tracepoints_perfetto.h)
set(INTEL_TRACEPOINTS_C ${INTEL_DS_GEN_DIR}/intel_tracepoints.c)

add_custom_command(
        OUTPUT ${INTEL_TRACEPOINTS_H} ${INTEL_TRACEPOINTS_PERFETTO_H} ${INTEL_TRACEPOINTS_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE}
        ${INTEL_DS_ROOT}/intel_tracepoints.py
        -p ${MESA_ROOT}/src/util/perf/
        --utrace-hdr ${INTEL_TRACEPOINTS_H}
        --perfetto-hdr ${INTEL_TRACEPOINTS_PERFETTO_H}
        --utrace-src ${INTEL_TRACEPOINTS_C}
        DEPENDS ${INTEL_DS_ROOT}/intel_tracepoints.py
        COMMENT "Generating intel/ds/intel_tracepoints.[h|c] (Python codegen)"
        VERBATIM
)

add_custom_target(mesa_intel_tracepoints_gen
        DEPENDS ${INTEL_TRACEPOINTS_H} ${INTEL_TRACEPOINTS_PERFETTO_H} ${INTEL_TRACEPOINTS_C}
)

# ────────────────────────────────────────────────────────────────
# idep_intel_driver_ds_headers — meson.build lines 36-40. In Meson
# eine reine declare_dependency() ohne eigene Kompilierung (nur
# Header + Include-Dirs + transitive Deps). Wir bilden das als
# INTERFACE-Target ab, das spaeter von Consumern verlinkt werden
# kann, ohne eine eigene Bibliothek zu sein.
# ────────────────────────────────────────────────────────────────
add_library(mesa_intel_ds_headers INTERFACE)
target_include_directories(mesa_intel_ds_headers INTERFACE
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/intel
        ${INTEL_DS_GEN_DIR}
)
add_dependencies(mesa_intel_ds_headers mesa_intel_tracepoints_gen)

# ────────────────────────────────────────────────────────────────
# libintel_driver_ds — meson.build lines 42-49.
# ────────────────────────────────────────────────────────────────
add_library(mesa_intel_ds STATIC
        ${INTEL_DS_ROOT}/intel_driver_ds.cc
        ${INTEL_TRACEPOINTS_C}
)

target_include_directories(mesa_intel_ds
        PUBLIC
        ${MESA_ROOT}/src
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src/intel
        ${MESA_ROOT}/src/intel/ds # needed for intel_tracepoints as it is getting build into the build dir and it has local references
        ${INTEL_DS_GEN_DIR}
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_intel_ds PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_C_FLAGS}>
)

target_compile_options(mesa_intel_ds PRIVATE -fvisibility=hidden)

target_link_libraries(mesa_intel_ds PUBLIC
        mesa_util
        mesa_nir
        mesa_intel_perf
        mesa_intel_common
        mesa_intel_dev
        mesa_intel_blorp
)

if(VESPERA_WITH_PERFETTO)
    # todo
endif()

add_dependencies(mesa_intel_ds
        sysroot_headers
        mesa_intel_tracepoints_gen
        mesa_util
        mesa_nir
        mesa_intel_dev
        mesa_intel_blorp
)

# ────────────────────────────────────────────────────────────────
# idep_intel_driver_ds — meson.build lines 51-55. Wieder ein reines
# INTERFACE-Target: transitive Include-Dirs + Link gegen die echte
# Bibliothek + die Header-only-Dependency von oben.
# ────────────────────────────────────────────────────────────────
add_library(mesa_intel_ds_dep INTERFACE)
target_link_libraries(mesa_intel_ds_dep INTERFACE
        mesa_intel_ds
        mesa_intel_ds_headers
)
target_include_directories(mesa_intel_ds_dep INTERFACE
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src/intel
)

# ────────────────────────────────────────────────────────────────
# pps-intel — meson.build lines 57-83. NICHT PORTIERT, siehe
# Header-Kommentar (haengt an Perfetto UND libdrm, beide noch nicht
# im Cross-Build verfuegbar). Absichtlich weggelassen:
#
# if(VESPERA_WITH_PERFETTO)
#     add_library(mesa_pps_intel STATIC
#             ${INTEL_DS_ROOT}/intel_pps_perf.cc
#             ${INTEL_DS_ROOT}/intel_pps_driver.cc
#     )
#     target_link_libraries(mesa_pps_intel PUBLIC
#             perfetto libdrm mesa_util mesa_intel_dev
#     )
#     target_compile_definitions(mesa_pps_intel PUBLIC PPS_INTEL)
# endif()
# ────────────────────────────────────────────────────────────────