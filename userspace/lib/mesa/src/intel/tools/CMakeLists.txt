# ════════════════════════════════════════════════════════════════
# lib/mesa/src/intel/tools/CMakeLists.txt
#
# Modeled on upstream src/intel/tools/meson.build.
#
# Bewusst ausgelassen ggü. upstream:
#   - idep_intel_decoder_elk / idep_intel_compiler_elk in
#     libintel_tools' dependencies: elk (Gen4-8-Backend) ist im
#     compiler-Layer bereits komplett ausgeschlossen (siehe
#     lib/mesa/src/intel/compiler/CMakeLists.txt Header-Kommentar).
#     libintel_tools verliert dadurch nur ELK-spezifische
#     Decode-Fähigkeiten in aubinator/error-decode — die generischen
#     (BRW/gen-basierten) Pfade bleiben.
#   - aubinator_viewer, intel_eu_stall_viewer, intel_hang_viewer
#     (with_tools.contains('intel-ui')): brauchen imgui, das nirgends
#     in diesem Baum existiert.
#   - intel_dev_info executable (with_intel_tools gate): eigenes
#     "Tool aktivieren"-Flag, hier nicht gesetzt — kann bei Bedarf
#     leicht ergänzt werden, da idep_intel_compiler_brw bereits
#     verfügbar ist (mesa_intel_compiler_brw-Target existiert).
#   - drm-shim-Zweig (with_tools.contains('drm-shim')): dep_drm_shim,
#     dep_libdrm — kein DRM-Äquivalent geplant (siehe intel/dev).
#   - intel_measure.py install_data: reines Installations-Statement,
#     kein Build-Artefakt.
#
# aub_write.c ist NICHT hier — meson.build listet es nur bei
# error2aub/error2hangdump direkt in deren eigenen files(), nicht in
# libintel_tools selbst.
# ════════════════════════════════════════════════════════════════

set(INTEL_TOOLS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# ────────────────────────────────────────────────────────────────
# libintel_tools — meson.build lines 4-27.
# ────────────────────────────────────────────────────────────────
set(INTEL_TOOLS_LIB_SOURCES
        aub_mem.c
        aub_read.c
        intel_tools.c
)
list(TRANSFORM INTEL_TOOLS_LIB_SOURCES PREPEND "${INTEL_TOOLS_ROOT}/")

add_library(mesa_intel_tools STATIC ${INTEL_TOOLS_LIB_SOURCES})

target_include_directories(mesa_intel_tools
        PUBLIC
        ${INTEL_TOOLS_ROOT}
        ${MESA_ROOT}/src
        ${MESA_ROOT}/include
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_intel_tools PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_CXX_FLAGS}>
)

target_compile_options(mesa_intel_tools PRIVATE -fvisibility=hidden)

# idep_intel_decoder_elk / idep_intel_compiler_elk bewusst NICHT
# gelinkt — siehe Header-Kommentar.
target_link_libraries(mesa_intel_tools PUBLIC
        mesa_util
        mesa_intel_dev
        mesa_intel_decoder_gen
        mesa_intel_compiler_brw
        mesa_intel_common
)

add_dependencies(mesa_intel_tools sysroot_headers mesa_util mesa_intel_dev)

# ────────────────────────────────────────────────────────────────
# aubinator — meson.build lines 37-46. Braucht dep_expat (XML-Parser)
# zusätzlich — noch nicht geprüft, ob vesplib das hat. Wenn nicht,
# schlägt das hier als Compile-/Link-Fehler auf, genauso wie ihr das
# bei anderen Layern bewusst als Signal genutzt habt statt vorab zu
# stubben.
# ────────────────────────────────────────────────────────────────
add_executable(mesa_intel_aubinator ${INTEL_TOOLS_ROOT}/aubinator.c)

target_include_directories(mesa_intel_aubinator PRIVATE
        ${INTEL_TOOLS_ROOT}
        ${MESA_ROOT}/src
        ${MESA_ROOT}/include
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_intel_aubinator PRIVATE ${MESA_COMMON_C_FLAGS})

target_link_libraries(mesa_intel_aubinator PRIVATE
        mesa_util
        mesa_intel_dev
        mesa_intel_common
        mesa_intel_tools
)

add_dependencies(mesa_intel_aubinator sysroot_headers mesa_intel_tools)

# ────────────────────────────────────────────────────────────────
# aubinator_error_decode — meson.build lines 48-65.
# ────────────────────────────────────────────────────────────────
set(INTEL_AUBINATOR_ERROR_DECODE_SOURCES
        aubinator_error_decode.c
        aubinator_error_decode_lib.c
        aubinator_error_decode_xe.c
        error_decode_lib.c
        error_decode_xe_lib.c
)
list(TRANSFORM INTEL_AUBINATOR_ERROR_DECODE_SOURCES PREPEND "${INTEL_TOOLS_ROOT}/")

add_executable(mesa_intel_aubinator_error_decode ${INTEL_AUBINATOR_ERROR_DECODE_SOURCES})

target_include_directories(mesa_intel_aubinator_error_decode PRIVATE
        ${INTEL_TOOLS_ROOT}
        ${MESA_ROOT}/src
        ${MESA_ROOT}/include
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_intel_aubinator_error_decode PRIVATE ${MESA_COMMON_C_FLAGS})

target_link_libraries(mesa_intel_aubinator_error_decode PRIVATE
        mesa_util
        mesa_intel_dev
        mesa_intel_common
        mesa_intel_tools
)

add_dependencies(mesa_intel_aubinator_error_decode sysroot_headers mesa_intel_tools)

# ────────────────────────────────────────────────────────────────
# intel_error2aub — meson.build lines 67-79.
# ────────────────────────────────────────────────────────────────
set(INTEL_ERROR2AUB_SOURCES
        aub_write.c
        error2aub.c
        error_decode_lib.c
)
list(TRANSFORM INTEL_ERROR2AUB_SOURCES PREPEND "${INTEL_TOOLS_ROOT}/")

add_executable(mesa_intel_error2aub ${INTEL_ERROR2AUB_SOURCES})

target_include_directories(mesa_intel_error2aub PRIVATE
        ${INTEL_TOOLS_ROOT}
        ${MESA_ROOT}/src
        ${MESA_ROOT}/include
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_intel_error2aub PRIVATE ${MESA_COMMON_C_FLAGS})

target_link_libraries(mesa_intel_error2aub PRIVATE mesa_util mesa_intel_dev)

add_dependencies(mesa_intel_error2aub sysroot_headers mesa_util mesa_intel_dev)

# ────────────────────────────────────────────────────────────────
# intel_error2hangdump — meson.build lines 81-99.
# ────────────────────────────────────────────────────────────────
set(INTEL_ERROR2HANGDUMP_SOURCES
        error2hangdump.c
        error2hangdump_lib.c
        error2hangdump_xe_lib.c
        error2hangdump_xe.c
        error_decode_lib.c
        error_decode_xe_lib.c
)
list(TRANSFORM INTEL_ERROR2HANGDUMP_SOURCES PREPEND "${INTEL_TOOLS_ROOT}/")

add_executable(mesa_intel_error2hangdump ${INTEL_ERROR2HANGDUMP_SOURCES})

target_include_directories(mesa_intel_error2hangdump PRIVATE
        ${INTEL_TOOLS_ROOT}
        ${MESA_ROOT}/src
        ${MESA_ROOT}/include
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_intel_error2hangdump PRIVATE ${MESA_COMMON_C_FLAGS})

target_link_libraries(mesa_intel_error2hangdump PRIVATE mesa_util mesa_intel_dev)

add_dependencies(mesa_intel_error2hangdump sysroot_headers mesa_util mesa_intel_dev)

# ────────────────────────────────────────────────────────────────
# intel_hang_replay — meson.build lines 101-114. Braucht dep_libdrm
# laut Original — bei euch kein libdrm vorhanden (siehe intel/dev
# Header-Kommentar zum LIBDRM SWITCH). Auskommentiert lassen, bis
# geklärt ist, ob intel_hang_replay.c tatsächlich echte DRM-Calls
# macht oder nur den Header-Typ braucht.
# ────────────────────────────────────────────────────────────────
# set(INTEL_HANG_REPLAY_SOURCES
#         intel_hang_replay.c
#         intel_hang_replay_lib.c
#         intel_hang_replay_xe.c
# )
# list(TRANSFORM INTEL_HANG_REPLAY_SOURCES PREPEND "${INTEL_TOOLS_ROOT}/")
# add_executable(mesa_intel_hang_replay ${INTEL_HANG_REPLAY_SOURCES})
# ... (siehe LIBDRM SWITCH Muster in intel/dev/CMakeLists.txt, falls aktiviert)

# ────────────────────────────────────────────────────────────────
# intel_sanitize_gpu — meson.build lines 145-155. SHARED lib laut
# Original (LD_PRELOAD-artiges Sanitize-Tool). sse2_args ergänzt.
# ────────────────────────────────────────────────────────────────
add_library(mesa_intel_sanitize_gpu SHARED ${INTEL_TOOLS_ROOT}/intel_sanitize_gpu.c)

target_include_directories(mesa_intel_sanitize_gpu PRIVATE
        ${INTEL_TOOLS_ROOT}
        ${MESA_ROOT}/src
        ${MESA_ROOT}/include
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_intel_sanitize_gpu PRIVATE ${MESA_COMMON_C_FLAGS} ${MESA_SSE2_ARGS})
target_compile_options(mesa_intel_sanitize_gpu PRIVATE -fvisibility=hidden)

target_link_libraries(mesa_intel_sanitize_gpu PRIVATE mesa_util mesa_intel_dev mesa_intel_common)

add_dependencies(mesa_intel_sanitize_gpu sysroot_headers mesa_util mesa_intel_dev)

# ────────────────────────────────────────────────────────────────
# intel_dump_gpu — meson.build lines 165-177.
# ────────────────────────────────────────────────────────────────
add_library(mesa_intel_dump_gpu SHARED ${INTEL_TOOLS_ROOT}/aub_write.c ${INTEL_TOOLS_ROOT}/intel_dump_gpu.c)

target_include_directories(mesa_intel_dump_gpu PRIVATE
        ${INTEL_TOOLS_ROOT}
        ${MESA_ROOT}/src
        ${MESA_ROOT}/include
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_intel_dump_gpu PRIVATE ${MESA_COMMON_C_FLAGS})
target_compile_options(mesa_intel_dump_gpu PRIVATE -fvisibility=hidden)

target_link_libraries(mesa_intel_dump_gpu PRIVATE mesa_util mesa_intel_dev mesa_intel_common)

add_dependencies(mesa_intel_dump_gpu sysroot_headers mesa_util mesa_intel_dev)

# ────────────────────────────────────────────────────────────────
# Aggregat für diesen Layer — analog zum "mesa"-Root-Aggregat.
# ────────────────────────────────────────────────────────────────
add_custom_target(mesa_intel_tools_all
        DEPENDS
        mesa_intel_tools
        mesa_intel_aubinator
        mesa_intel_aubinator_error_decode
        mesa_intel_error2aub
        mesa_intel_error2hangdump
        mesa_intel_sanitize_gpu
        mesa_intel_dump_gpu
        COMMENT "All active intel/tools executables and libraries"
)