# lib/mesa/src/intel/common/CMakeLists.txt
#
# Portierung von src/intel/common/meson.build.
#
# WICHTIG -- bewusst ausgelassen ggue. upstream:
#   - i915/*.c, i915/*.h : Kernel-Mode-Driver-Anbindung fuer den Linux
#     i915-Treiber (echte ioctl()-Aufrufe gegen /dev/dri/*, libdrm-Semantik).
#     Genau das, was ihr laut Projektbeginn NICHT wollt -- ihr habt einen
#     eigenen Kernel-Treiber und implementiert iris_kmd_backend fuer eure
#     eigene GPU-API, nicht i915. Diese Dateien wuerden gegen echtes
#     libdrm/i915-uAPI linken, was bei euch nicht existiert.
#   - xe/*.c, xe/*.h : gleiches Prinzip, nur fuer den neueren Xe-KMD.
#     Ebenfalls nicht relevant -- ihr simuliert weder i915 noch xe.
#   - dep_libdrm : komplett raus, aus genau demselben Grund.
#   - Tests (mi_builder_test), with_platform_android (dep_android).
#
# Die Dateien, die hier bleiben (intel_aux_map, intel_measure, intel_uuid,
# intel_l3_config, intel_common.c, etc.) sind KMD-agnostisch -- sie
# arbeiten mit Datenstrukturen/Algorithmen, nicht mit ioctl()-Aufrufen.
# Der Punkt, an dem euer eigener Kernel-Treiber tatsaechlich angebunden
# wird, ist NICHT hier, sondern im iris-winsys-Layer (iris_kmd_backend,
# siehe Projektbeginn) -- deutlich weiter oben in der Kette.

set(INTEL_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(MESA_INTEL_COMMON_SOURCES
        intel_aux_map.c
        # intel_bind_timeline.c specific for XE, requires DRM, so we exclude it here
        intel_common.c
        intel_compute_slm.c
        intel_debug_identifier.c
        intel_engine.c
        intel_gem.c
        intel_l3_config.c
        intel_urb_config.c
        intel_sample_positions.c
        intel_uuid.c
        intel_measure.c
        intel_pagefault.c
)

add_library(mesa_intel_common STATIC ${MESA_INTEL_COMMON_SOURCES})

target_include_directories(mesa_intel_common
        PUBLIC
        ${INTEL_COMMON_DIR}
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/intel
        ${GENXML_INCLUDE_DIRS}
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_link_libraries(mesa_intel_common
        PUBLIC
        mesa_util
        mesa_intel_dev
        mesa_isl
        PRIVATE
        pthread
)

target_compile_options(mesa_intel_common PRIVATE ${MESA_COMMON_C_FLAGS})

add_dependencies(mesa_intel_common sysroot_headers mesa_genxml mesa_isl)

install(TARGETS mesa_intel_common ARCHIVE DESTINATION ${VESPERA_SYSROOT}/usr/lib COMPONENT runtime)