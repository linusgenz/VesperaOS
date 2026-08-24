# src/mesa/es2api/CMakeLists.txt
#

set(ES2API_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(ES2API_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

# --- 1. Codegen: es2_glapi_mapi_tmp.h ---------------------------------------
set(GEN_GLDISPATCH_MAPI_PY ${ES2API_ROOT}/../new/gen_gldispatch_mapi.py)
set(GL_REGISTRY_XML ${ES2API_ROOT}/../glapi/registry/gl.xml)
set(ES2_GLAPI_MAPI_TMP_H ${ES2API_OUT_DIR}/es2_glapi_mapi_tmp.h)

add_custom_command(
        OUTPUT ${ES2_GLAPI_MAPI_TMP_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_GLDISPATCH_MAPI_PY}
        glesv2
        ${GL_REGISTRY_XML}
        > ${ES2_GLAPI_MAPI_TMP_H}
        DEPENDS
        ${GEN_GLDISPATCH_MAPI_PY}
        ${GL_REGISTRY_XML}
        ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating es2_glapi_mapi_tmp.h"
        VERBATIM
)

# --- 3. Target: libGLESv2 ---------------------------------------------------
add_library(GLESv2 STATIC
        $<TARGET_OBJECTS:dso_handle_stub>
        ${ES2API_ROOT}/libgles2_public.c
        ${ES2_GLAPI_MAPI_TMP_H}
)

# Properties: SOVERSION, VERSION, Output-Name & Symbol-Visibility
set_target_properties(GLESv2 PROPERTIES
        OUTPUT_NAME "GLESv2${GLES_LIB_SUFFIX}"
        VERSION "2.0.0"
        SOVERSION "2"
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

target_include_directories(GLESv2 PRIVATE
        ${ES2API_ROOT}
        ${ES2API_OUT_DIR}
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/mesa
)

target_link_libraries(GLESv2 PRIVATE
        vesplib
        -Wl,--whole-archive $<TARGET_FILE:vesplib_cpp> -Wl,--no-whole-archive
        glapi                   # Das Target aus shared-glapi/CMakeLists.txt
        # ${DEP_LIBDRM}
        ${CMAKE_DL_LIBS}        # Entspricht dep_dl
        mesa_util_c11
        mesa_util
        mesa
)

target_compile_options(GLESv2 PRIVATE ${MESA_COMMON_CXX_FLAGS})

target_link_options(GLESv2 PRIVATE
        ${MESA_COMMON_CXX_FLAGS}
        -nostdlib
        -nodefaultlibs
        -Wl,-dynamic-linker,/lib/ld-vespera.so
)

if(LD_ARGS_GC_SECTIONS)
    target_link_options(GLESv2 PRIVATE ${LD_ARGS_GC_SECTIONS})
endif()

install(TARGETS GLESv2
        LIBRARY DESTINATION ${VESPERA_SYSROOT}/usr/lib
        ARCHIVE DESTINATION ${VESPERA_SYSROOT}/usr/lib
        RUNTIME DESTINATION ${VESPERA_SYSROOT}/usr/bin
        COMPONENT runtime
)