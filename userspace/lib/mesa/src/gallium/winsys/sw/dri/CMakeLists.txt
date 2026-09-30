add_library(swdri STATIC
        dri_sw_winsys.c
        dri_sw_winsys.h
)

set_target_properties(swdri PROPERTIES
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

target_include_directories(swdri PRIVATE
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/gallium/auxiliary
)

target_compile_options(swdri PRIVATE
        ${MESA_COMMON_C_FLAGS}
)

target_link_libraries(swdri PRIVATE
        mesa_util
)