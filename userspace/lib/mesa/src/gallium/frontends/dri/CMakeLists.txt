# src/gallium/frontends/dri/CMakeLists.txt
#
# Konvertiert aus src/gallium/frontends/dri/meson.build. Reduziert:
#   - kein with_platform_x11 (kein dri3-helper, kein xcb-Zeug)
#   - with_dri = true          -> drisw.c mit rein
#   - with_dri2 = true         -> dri2.c mit rein (braucht egl_dri2.c ja
#                                  für den Wayland/DRM-Fall)
#   - with_gallium_zink = false -> kopper_stubs.c statt kopper.c
#     (falls ihr später Zink/kopper wollt, sag Bescheid, ist ein
#     eigener Zweig mit vulkan_wsi_deps den ich hier weggelassen habe)
#   - kein glvnd
#   - kein pkg-config-Generate
#
# sha1_h aus dem meson-File ist eine generierte Version-Info-Datei
# (git-sha1.h o.ä.) — laut dir irrelevant, daher hier weggelassen.
# Falls dri_util.c/dri_screen.c das per #include erwarten und ohne
# nicht baut, sag Bescheid, dann bau ich einen Dummy-generator dafür.

set(DRI_FRONTEND_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(DRI_FRONTEND_SOURCES
        dri_context.c
        dri_drawable.c
        dri_helpers.c
        dri_query_renderer.c
        dri_screen.c
        dri_util.c
)

# with_dri: software-rendering entrypoint (drisw). Bleibt drin, auch
# wenn ihr nur den Hardware-Pfad (iris) nutzt — drisw.c wird nur bei
# tatsächlicher swrast-Nutzung aktiv, kostet aber nichts, wenn
# ungenutzt. Sag Bescheid falls du das rausnehmen willst.
list(APPEND DRI_FRONTEND_SOURCES drisw.c)

# with_dri2: der eigentliche DRI2/Wayland/DRM-Pfad, den egl_dri2.c
# braucht.
list(APPEND DRI_FRONTEND_SOURCES dri2.c)

# with_gallium_zink = false -> Stub statt kopper.c (keine
# Vulkan-WSI-Abhängigkeit)
list(APPEND DRI_FRONTEND_SOURCES kopper_stubs.c)

add_library(mesa_dri_frontend OBJECT ${DRI_FRONTEND_SOURCES})

set_target_properties(mesa_dri_frontend PROPERTIES
        C_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(mesa_dri_frontend PRIVATE
        ${DRI_FRONTEND_DIR}
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/util
        ${MESA_ROOT}/src/mesa
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/src/gallium/auxiliary
        ${MESA_ROOT}/src/gallium/drivers
        ${MESA_ROOT}/src/loader
)

target_compile_options(mesa_dri_frontend PRIVATE
        ${MESA_COMMON_C_FLAGS}
)

# dependencies: dep_libdrm, idep_mesautil (mesa_util), libloader
# (link_with in meson -> target_link_libraries hier, PUBLIC weil
# dri_util.h Loader-Typen in seiner API-Signatur durchreicht — falls
# das bei euch nicht der Fall ist, kann's auch PRIVATE sein)
target_link_libraries(mesa_dri_frontend PUBLIC
        mesa_util
        mesa_loader
        mesa_gallium_aux
)

add_dependencies(mesa_dri_frontend host-tools)