# src/egl/CMakeLists.txt
#
# Konvertiert aus src/egl/meson.build. Reduziert auf den Pfad, den
# VesperaOS tatsächlich braucht:
#   - with_dri = true (libGLESv2 / gallium_dri Pfad)
#   - with_platform_wayland = true, alle anderen Plattformen
#     (x11, android, haiku, windows) entfernt
#   - kein glvnd (with_glvnd = false)
#   - with_wayland_bind_display = false (kein wayland-drm-Zusatzprotokoll)
#   - kein symbols-check, kein pkg-config-Generate, kein VS-Module-Def
#
# git_sha1 / libc-Spezifika sind hier absichtlich nicht behandelt, wie
# besprochen. libdrm wird als bereits vorhandenes Target vorausgesetzt
# (Ziel: drm::drm oder libdrm, an TARGET-Namen unten anpassen falls
# abweichend).

set(EGL_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(EGL_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${EGL_GEN_DIR})

# ────────────────────────────────────────────────────────────────
# Quellen (main/ egl core + drivers/dri2 wayland-Teilmenge)
# ────────────────────────────────────────────────────────────────
set(EGL_MAIN_SOURCES
        main/eglapi.c
        main/eglarray.c
        main/eglconfigdebug.c
        main/eglconfig.c
        main/eglcontext.c
        main/eglcurrent.c
        main/egldevice.c
        main/egldisplay.c
        main/eglglobals.c
        main/eglimage.c
        main/egllog.c
        main/eglsurface.c
        main/eglsync.c
)

# with_dri: dri2 core + platform_device/platform_surfaceless sind
# immer mit dabei, platform_wayland kommt dazu (kein platform_x11,
# kein platform_drm/gbm — falls du GBM/DRM-KMS-Output auch über EGL
# willst statt nur Wayland, sag Bescheid, das ist ein eigener Zweig
# in der meson.build den ich hier weggelassen habe)
set(EGL_DRI2_SOURCES
        drivers/dri2/egl_dri2.c
        drivers/dri2/platform_device.c
        drivers/dri2/platform_surfaceless.c
        drivers/dri2/platform_wayland.c
)

# ────────────────────────────────────────────────────────────────
# g_egldispatchstubs — generate/gen_egl_dispatch.py aus egl.xml +
# egl_other.xml. gen_args bleibt leer: with_wayland_bind_display=false
# heißt --bind-wl-display wird NICHT übergeben.
# ────────────────────────────────────────────────────────────────
set(EGL_DISPATCH_C ${EGL_GEN_DIR}/g_egldispatchstubs.c)
set(EGL_DISPATCH_H ${EGL_GEN_DIR}/g_egldispatchstubs.h)

add_custom_command(
        OUTPUT ${EGL_DISPATCH_C} ${EGL_DISPATCH_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${EGL_DIR}/generate/gen_egl_dispatch.py
        --source ${EGL_DISPATCH_C}
        --header ${EGL_DISPATCH_H}
        ${EGL_DIR}/generate/egl.xml
        ${EGL_DIR}/generate/egl_other.xml
        DEPENDS
        ${EGL_DIR}/generate/gen_egl_dispatch.py
        ${EGL_DIR}/generate/eglFunctionList.py
        ${EGL_DIR}/generate/egl.xml
        ${EGL_DIR}/generate/egl_other.xml
        COMMENT "Generating g_egldispatchstubs.c/.h"
        VERBATIM
)

# Nur ins Ziel aufnehmen, wenn dein with_glvnd-Äquivalent an ist —
# bei euch ist glvnd komplett raus, daher NICHT zu EGL_MAIN_SOURCES
# hinzufügen (meson tut das auch nur im `else`-Zweig von with_glvnd).
# Datei bleibt oben generiert falls du sie später doch brauchst;
# wenn du sie nie brauchst, kann der custom_command block auch ganz
# raus — sag Bescheid.

# ────────────────────────────────────────────────────────────────
# libEGL
# ────────────────────────────────────────────────────────────────
add_library(egl STATIC
        ${EGL_MAIN_SOURCES}
        ${EGL_DRI2_SOURCES}
)

set_target_properties(egl PROPERTIES
        OUTPUT_NAME "EGL"
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

target_include_directories(egl PRIVATE
        ${EGL_DIR}
        ${EGL_DIR}/main
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/loader
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/src/gallium/auxiliary
        ${MESA_ROOT}/src/gallium/frontends/dri
        ${MESA_ROOT}/src/mesa
        ${MESA_ROOT}/src/mesa/state_tracker
        ${CMAKE_CURRENT_BINARY_DIR}/../mesa/glapi/glapi/gen
        $<TARGET_PROPERTY:wayland-protocols,INTERFACE_INCLUDE_DIRECTORIES>
        ${PROJECT_SOURCE_DIR}/wayland/wayland/egl
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_loader PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
)

target_compile_definitions(egl PRIVATE
        _EGL_NATIVE_PLATFORM=_EGL_PLATFORM_WAYLAND
        WL_EGL_PLATFORM=1
        HAVE_WAYLAND_PLATFORM
)

target_compile_options(egl PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_CXX_FLAGS}>
)

# with_ld_version_script (egl.sym), kein glvnd-Symbolfile
target_link_options(egl PRIVATE
        -Wl,--version-script,${EGL_DIR}/egl.sym
        -Wl,--gc-sections
)
set_target_properties(egl PROPERTIES LINK_DEPENDS ${EGL_DIR}/egl.sym)

target_link_libraries(egl PRIVATE
        mesa_gallium_aux
        mesa_dri_frontend
        mesa_loader
        mesa_loader_wayland_helper
        mesa_iris_driver
        glapi
        GLESv2
        mesa
        wayland-client
        wayland-server
        wayland-egl
        wayland-protocols-code
)

add_custom_target(mesa_egl_dispatch_gen DEPENDS ${EGL_DISPATCH_C} ${EGL_DISPATCH_H})
add_dependencies(egl mesa_egl_dispatch_gen GLESv2)

install(FILES       $<TARGET_FILE:egl>
        DESTINATION "${VESPERA_SYSROOT}/usr/lib"
        COMPONENT   runtime
)