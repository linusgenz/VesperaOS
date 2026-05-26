set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SYSROOT ${CMAKE_SOURCE_DIR}/sysroot)

set(COMMON_C_FLAGS
        "-ffreestanding -nostdlib -nostdinc -fno-stack-protector -fPIE -g -O0 -Wall \
     --sysroot=${SYSROOT} \
     -isystem ${SYSROOT}/usr/include"
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_C_FLAGS}")

set(USER_LD_FLAGS
        "-nostdlib -ffreestanding -fno-stack-protector -pie \
     --sysroot=${SYSROOT} \
     -L${SYSROOT}/usr/lib"
)

add_subdirectory(lib)


function(vespera_bin name)
    cmake_parse_arguments(ARG "READLINE;LVGL" "" "SOURCES;EXTRA_LIBS" ${ARGN})

    if (ARG_SOURCES)
        set(sources ${ARG_SOURCES})
    else ()
        set(sources ${CMAKE_CURRENT_SOURCE_DIR}/bin/${name}.c)
    endif ()

    add_executable(${name} ${sources})

    set(_link_libs "")
    if (ARG_EXTRA_LIBS)
        list(APPEND _link_libs ${ARG_EXTRA_LIBS})
    endif ()
    if (ARG_READLINE)
        list(APPEND _link_libs ${SYSROOT}/usr/lib/libreadline.a)
        add_dependencies(${name} sysroot_install sysroot_install_readline)
    else ()
        add_dependencies(${name} sysroot_install)
    endif ()

    if (ARG_LVGL)
        list(APPEND _link_libs ${SYSROOT}/usr/lib/liblvgl.a)

        add_dependencies(${name} sysroot_install_lvgl)

        target_include_directories(${name} PRIVATE
                ${SYSROOT}/usr/include/lvgl
        )
    endif ()

    list(APPEND _link_libs ${SYSROOT}/usr/lib/libvesp.a)

    target_link_libraries(${name} PRIVATE ${_link_libs})

    set_target_properties(${name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${SYSROOT}/bin
            LINK_FLAGS "${USER_LD_FLAGS}"
    )

    # Self-register so 'elfs' never needs a manual update
    set_property(GLOBAL APPEND PROPERTY VESPERA_BIN_TARGETS ${name})
endfunction()

# ---------------------------------------------------------------------------
# Explicit registrations
# Only needed for: multi-source binaries, READLINE, EXTRA_LIBS, or aliased
# sources (multiple targets from the same .c file).
#
# Single-source binaries that follow the default bin/<name>.c convention are
# auto-discovered below — just add the .c file, nothing else.
# ---------------------------------------------------------------------------

# Multi-source
vespera_bin(nox
        READLINE
        SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/nox/nox.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/nox/statusbar.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/nox/powermon.c
)

vespera_bin(env SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/bin/env.c)
vespera_bin(printenv SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/bin/env.c)

add_executable(lua ${CMAKE_CURRENT_SOURCE_DIR}/bin/lua.c)
add_dependencies(lua sysroot_install sysroot_install_readline)
target_include_directories(lua PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/lua/include
        ${SYSROOT}/usr/include
)
target_link_libraries(lua PRIVATE
        liblvespera
        liblua
        ${SYSROOT}/usr/lib/libreadline.a
        ${SYSROOT}/usr/lib/libvesp.a
)
set_target_properties(lua PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${SYSROOT}/bin
        LINK_FLAGS "${USER_LD_FLAGS}"
)
set_property(GLOBAL APPEND PROPERTY VESPERA_BIN_TARGETS lua)

vespera_bin(crepus
        LVGL
        SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/crepus.c
)

# ---------------------------------------------------------------------------
# Auto-discovery
# Every bin/<name>.c that is NOT in the explicit list above becomes a
# vespera_bin(<name>) automatically.
# ---------------------------------------------------------------------------
set(_EXPLICIT_BINS nox env printenv lua crepus)

file(GLOB _auto_sources CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/bin/*.c
)

foreach (_src ${_auto_sources})
    get_filename_component(_name ${_src} NAME_WE)
    if (NOT _name IN_LIST _EXPLICIT_BINS)
        vespera_bin(${_name})
    endif ()
endforeach ()

get_property(_all_bins GLOBAL PROPERTY VESPERA_BIN_TARGETS)
add_custom_target(elfs DEPENDS ${_all_bins})