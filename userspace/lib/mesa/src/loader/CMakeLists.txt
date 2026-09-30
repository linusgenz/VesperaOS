# src/loader/CMakeLists.txt
#
# Konvertiert aus src/loader/meson.build. Reduziert:
#   - with_platform_wayland = true, alle anderen with_platform_* weg
#   - Von den 7 in meson gesuchten Wayland-Protokollen (fifo,
#     commit-timing, linux-dmabuf, presentation-time, tearing-control,
#     linux-drm-syncobj, color-management) wird in DIESER Datei
#     tatsächlich nur presentation-time als Quelle kompiliert
#     (wp_files['presentation-time'] in libloader_wayland_helper) —
#     die anderen 6 werden in meson nur gescannt/gefunden, aber nirgends
#     in diesem File als Source verwendet (linux-dmabuf taucht z.B.
#     nochmal separat in egl/meson.build auf, siehe Kommentar unten).
#     Ich hab daher nur presentation-time hier per wayland-scanner
#     generiert. Falls du fifo/commit-timing/tearing-control/
#     linux-drm-syncobj/color-management später doch brauchst (z.B.
#     für Crepusculum-Sync), sag Bescheid.
#   - with_dri2 -> loader_c_args (-DUSE_DRICONF) übernommen, da with_dri
#     bei euch aktiv ist
#   - kein xcb/xcb_xrandr (kein with_platform_x11)
#   - sha1_h wie überall weggelassen (dein Hinweis: irrelevant)
#   - build_by_default:false gibt's in CMake nicht direkt; beide Libs
#     werden nur gebaut, wenn tatsächlich von etwas verlinkt (normales
#     CMake-Verhalten für Libraries ohne ALL-Target-Abhängigkeit)
#
# Wayland-Protokoll-XMLs: ich gehe davon aus, dass presentation-time.xml
# über euer wayland-protocols-Sysroot-Verzeichnis auffindbar ist
# (WAYLAND_PROTOCOLS_DIR unten) — Pfad ggf. anpassen, ich kenne den
# exakten Ort in eurem Repo nicht.

set(LOADER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LOADER_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${LOADER_GEN_DIR})

if(NOT DEFINED WAYLAND_SCANNER_EXECUTABLE)
    find_program(WAYLAND_SCANNER_EXECUTABLE wayland-scanner REQUIRED)
endif()

if(NOT DEFINED WAYLAND_PROTOCOLS_DIR)
    message(FATAL_ERROR
            "loader/CMakeLists.txt: WAYLAND_PROTOCOLS_DIR ist nicht gesetzt "
            "(erwartet z.B. .../wayland-protocols/staging/presentation-time/"
            "presentation-time.xml darunter). Bitte im Root-CMakeLists setzen "
            "oder hier den echten Pfad eintragen."
    )
endif()

set(PRESENTATION_TIME_XML
        ${WAYLAND_PROTOCOLS_DIR}/stable/presentation-time/presentation-time.xml)

set(PRESENTATION_TIME_PROTOCOL_C ${LOADER_GEN_DIR}/presentation-time-protocol.c)
set(PRESENTATION_TIME_CLIENT_H ${LOADER_GEN_DIR}/presentation-time-client-protocol.h)

add_custom_command(
        OUTPUT ${PRESENTATION_TIME_PROTOCOL_C}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code
        ${PRESENTATION_TIME_XML} ${PRESENTATION_TIME_PROTOCOL_C}
        DEPENDS ${PRESENTATION_TIME_XML}
        COMMENT "Generating presentation-time-protocol.c"
        VERBATIM
)
add_custom_command(
        OUTPUT ${PRESENTATION_TIME_CLIENT_H}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header
        ${PRESENTATION_TIME_XML} ${PRESENTATION_TIME_CLIENT_H}
        DEPENDS ${PRESENTATION_TIME_XML}
        COMMENT "Generating presentation-time-client-protocol.h"
        VERBATIM
)

add_custom_target(loader_presentation_time_gen
        DEPENDS ${PRESENTATION_TIME_PROTOCOL_C} ${PRESENTATION_TIME_CLIENT_H}
)

# ────────────────────────────────────────────────────────────────
# libloader_wayland_helper (nur bei with_platform_wayland gebaut)
# ────────────────────────────────────────────────────────────────
add_library(mesa_loader_wayland_helper OBJECT
        loader_wayland_helper.c
        ${PRESENTATION_TIME_PROTOCOL_C}
)
add_dependencies(mesa_loader_wayland_helper loader_presentation_time_gen)

set_target_properties(mesa_loader_wayland_helper PROPERTIES
        C_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(mesa_loader_wayland_helper PRIVATE
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/gallium/include
        ${LOADER_GEN_DIR}
)

target_compile_definitions(mesa_loader_wayland_helper PRIVATE
        HAVE_WL_DISPATCH_QUEUE_TIMEOUT
        HAVE_WL_CREATE_QUEUE_WITH_NAME
)

target_link_libraries(mesa_loader_wayland_helper PUBLIC
        mesa_util
        wayland-client
)

target_compile_options(mesa_loader_wayland_helper PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
)

# ────────────────────────────────────────────────────────────────
# libloader (immer gebaut)
# ────────────────────────────────────────────────────────────────
add_library(mesa_loader OBJECT
        loader_dri_helper.c
        loader.c
)

set_target_properties(mesa_loader PROPERTIES
        OUTPUT_NAME "loader"
        C_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(mesa_loader PRIVATE
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/util
        ${MESA_ROOT}/src/gallium/include
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_loader PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
)

target_compile_definitions(mesa_loader PRIVATE
        USE_DRICONF
)

target_link_libraries(mesa_loader PUBLIC
        mesa_util
        drm
)

add_dependencies(mesa_loader host-tools)