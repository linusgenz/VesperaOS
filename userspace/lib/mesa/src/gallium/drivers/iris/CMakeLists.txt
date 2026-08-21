# lib/mesa/src/gallium/drivers/iris/CMakeLists.txt
#
# Reduziertes Set gegenueber Mesas src/gallium/drivers/iris/meson.build:
# - kein i915/, kein xe/ (eigenes Lucifer-KMD statt i915/xe-Emulation,
#   siehe gallium/drivers/iris/lucifer/)
# - iris_batch.c und iris_bufmgr.c NOCH NICHT gebaut (Stub-Backend deckt
#   den KMD-Zugriff ab; Batch/Bufmgr-Anbindung folgt separat, laeuft bis
#   dahin ueber den `default: UNREACHABLE("missing");`-Case im jeweiligen
#   Aufrufer)
# - iris_per_hw_ver_libs (Meson-Foreach ueber GFX_VERx10 80/90/110/120/125/
#   200/300/350 fuer iris_blorp.c/iris_query.c/iris_state.c/
#   iris_indirect_gen.c) noch nicht nachgebaut; blorp/state/query aktuell
#   ausgeklammert, da sie an den Per-Hw-Ver-Mechanismus gekoppelt sind
#
# Ziel dieses Schritts ist nur die Build-Infrastruktur (Target existiert,
# linkt gegen die schon fertigen Layer). Fehlende .c-Dateien werden
# ergaenzt, sobald Batch/Bufmgr/Per-Hw-Ver angegangen werden.

set(IRIS_SOURCES
        driinfo_iris.h
        iris_binder.c
        iris_binder.h
        iris_blit.c
        iris_border_color.c
        iris_clear.c
        iris_context.c
        iris_context.h
        iris_draw.c
        iris_fence.c
        iris_fence.h
        iris_fine_fence.c
        iris_fine_fence.h
        iris_formats.c
        iris_genx_macros.h
        iris_genx_protos.h
        iris_kmd_backend.c
        iris_kmd_backend.h
        iris_measure.c
        iris_measure.h
        iris_monitor.c
        iris_nir_analyze_ubo_ranges.c
        iris_perf.h
        iris_perf.c
        iris_performance_query.c
        iris_pipe.h
        iris_pipe_control.c
        iris_program.c
        iris_program_cache.c
        iris_resolve.c
        iris_resource.c
        iris_resource.h
        iris_screen.c
        iris_screen.h
        iris_disk_cache.c
        iris_utrace.c

        # Eigenes KMD-Backend statt i915/iris_kmd_backend.c bzw.
        # xe/iris_kmd_backend.c
        lucifer/iris_kmd_backend.c
)

add_library(mesa_iris STATIC ${IRIS_SOURCES})

target_compile_options(mesa_iris PRIVATE ${MESA_COMMON_C_FLAGS})

target_compile_definitions(mesa_iris PUBLIC MAJOR_IN_SYSMACROS)

target_include_directories(mesa_iris PRIVATE
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/mapi
        ${MESA_ROOT}/src/mesa
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/src/gallium/auxiliary
        ${MESA_ROOT}/src/gallium/drivers
        ${MESA_ROOT}/src/intel
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mesa_iris PRIVATE
        mesa_util
        mesa_gallium_aux
        mesa_intel_dev
        mesa_intel_common
        mesa_intel_compiler_gen
        mesa_intel_compiler_nir
        mesa_intel_compiler_brw
        mesa_intel_blorp
        mesa_intel_decoder
        mesa_intel_isl
)

add_subdirectory(${MESA_ROOT}/src/gallium/winsys/iris/drm
        ${CMAKE_CURRENT_BINARY_DIR}/winsys-iris-drm)

add_custom_target(mesa_iris_driver
        DEPENDS
        mesa_iris
        mesa_iris_winsys
        COMMENT "Building iris gallium driver (lucifer backend, no batch/bufmgr/per-hw-ver yet)"
)