# lib/mesa/src/gallium/winsys/iris/drm/CMakeLists.txt
#
# Entspricht 1:1 der originalen meson.build dieses Ordners (nur
# iris_drm_winsys.c) — kein Layer-Umbau noetig, die Datei war schon
# minimal. iris_drm_probe_nctx() bleibt drin, ist aber toter Code, da
# HAVE_INTEL_VIRTIO bei uns nie definiert wird (kein Virtio-GPU).

add_library(mesa_iris_winsys STATIC
        iris_drm_winsys.c
)

target_compile_options(mesa_iris_winsys PRIVATE ${MESA_COMMON_C_FLAGS})

target_include_directories(mesa_iris_winsys PRIVATE
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/src/gallium/auxiliary
        ${MESA_ROOT}/src/gallium/drivers
        ${MESA_ROOT}/src/intel
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(mesa_iris_winsys PRIVATE
        mesa_intel_dev
        mesa_iris
        mesa_util
)