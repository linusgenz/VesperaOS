# src/mesa/glapi/shared-glapi/CMakeLists.txt
#
# 1:1-Uebersetzung von src/mesa/glapi/shared-glapi/meson.build.

set(SHARED_GLAPI_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(SHARED_GLAPI_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

set(MAPI_ABI_PY ${SHARED_GLAPI_ROOT}/../mapi_abi.py)
set(GL_AND_ES_API_XML ${SHARED_GLAPI_ROOT}/../glapi/gen/gl_and_es_API.xml)
set(LIBGL_SYMBOLS_TXT ${SHARED_GLAPI_ROOT}/../../../glx/libgl-symbols.txt)

set(SHARED_GLAPI_MAPI_TMP_H ${SHARED_GLAPI_OUT_DIR}/shared_glapi_mapi_tmp.h)

add_custom_command(
        OUTPUT ${SHARED_GLAPI_MAPI_TMP_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${MAPI_ABI_PY}
        --printer shared-glapi
        --gl_symbols ${LIBGL_SYMBOLS_TXT}
        ${GL_AND_ES_API_XML}
        > ${SHARED_GLAPI_MAPI_TMP_H}
        DEPENDS
        ${MAPI_ABI_PY}
        ${GL_AND_ES_API_XML}
        ${LIBGL_SYMBOLS_TXT}
        ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating shared_glapi_mapi_tmp.h"
        VERBATIM
)

set(GCC_LTO_QUIRK "")
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(GCC_LTO_QUIRK "-fno-lto")
endif()

# --- 4. Target: libglapi ----------------------------------------------------
add_library(glapi STATIC
        ${SHARED_GLAPI_ROOT}/core.c
        ${SHARED_GLAPI_MAPI_TMP_H}
)

# DMAPI_MODE_SHARED_GLAPI definieren
target_compile_definitions(glapi PRIVATE
        MAPI_MODE_SHARED_GLAPI
)

target_compile_options(glapi PRIVATE
        ${MESA_COMMON_C_FLAGS}
        -ftls-model=global-dynamic
)

# gnu_symbol_visibility : 'hidden'
set_target_properties(glapi PROPERTIES
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

target_include_directories(glapi PRIVATE
        ${SHARED_GLAPI_ROOT}
        ${SHARED_GLAPI_OUT_DIR}
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/mesa
)

if(C_MSVC_COMPAT_ARGS OR GCC_LTO_QUIRK)
    target_compile_options(glapi PRIVATE ${C_MSVC_COMPAT_ARGS} ${GCC_LTO_QUIRK})
endif()

target_link_libraries(glapi PRIVATE
        ${DEP_THREAD}
        ${IDEP_MESAUTIL}
)

if(LD_ARGS_GC_SECTIONS)
    target_link_options(glapi PRIVATE ${LD_ARGS_GC_SECTIONS})
endif()

set(LIBGLAPI_BUILD_DIR ${SHARED_GLAPI_OUT_DIR} PARENT_SCOPE)