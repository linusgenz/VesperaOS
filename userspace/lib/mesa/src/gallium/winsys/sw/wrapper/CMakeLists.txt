add_library(wsw STATIC
        wrapper_sw_winsys.c
)

set_target_properties(wsw PROPERTIES
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ON
        EXCLUDE_FROM_ALL TRUE
)

target_include_directories(wsw PRIVATE
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/gallium/auxiliary
)

target_compile_options(wsw PRIVATE
        ${MESA_COMMON_C_FLAGS}
)

target_link_libraries(wsw PRIVATE
        mesa_util
)