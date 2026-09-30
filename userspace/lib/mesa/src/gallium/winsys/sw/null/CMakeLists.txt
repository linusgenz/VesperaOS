add_library(ws_null STATIC
        null_sw_winsys.c
        null_sw_winsys.h
)

set_target_properties(ws_null PROPERTIES
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        EXCLUDE_FROM_ALL TRUE
)

target_include_directories(ws_null PRIVATE
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/gallium/auxiliary
        ${MESA_ROOT}/include
)

target_compile_options(ws_null PRIVATE
        ${MESA_COMMON_C_FLAGS}
)

target_link_libraries(ws_null PRIVATE
        mesa_util
)