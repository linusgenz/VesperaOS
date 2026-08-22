# src/mesa/glapi/glapi/gen/CMakeLists.txt
#
# 1:1-Uebersetzung von meson_glapi_glapi_gen.build. Das ist die eigentliche
# Codegen-Schicht fuer den GL-Dispatch: aus gl_and_es_API.xml / gl_API.xml
# werden per Python (mapi_abi.py, gl_table.py, gl_enums.py, api_exec_*.py,
# marshal_generated_*.py, glX_proto_*.py, ...) alle Header/Source erzeugt,
# die main/CMakeLists.txt (mesa_main) am Ende einsammelt.
#
# Voraussetzung: MESA_PYTHON3_EXECUTABLE ist bereits ueber FindMesaPython.cmake
# im Root-CMakeLists gesetzt. glapi_xml_py_deps kommt aus dem Parent-Scope
# (src/mesa/glapi/CMakeLists.txt).

set(GEN_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(GEN_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

# CMake kennt die Zielarchitektur bereits ueber CMAKE_SIZEOF_VOID_P (analog zu
# Meson's cross-file-abgeleitetem sizeof_pointer). VesperaOS ist reines
# x86_64 -> 8, aber wir leiten es sauber ab statt hart zu kodieren.
if(NOT CMAKE_SIZEOF_VOID_P)
    set(SIZEOF_POINTER 8)
else()
    set(SIZEOF_POINTER ${CMAKE_SIZEOF_VOID_P})
endif()

# Pfade zu den XML-Quellen, wie im Meson-Baum relativ referenziert:
#   gen/                    -> gl_and_es_API.xml, gl_API.xml liegen hier
#   glapi/registry/gl.xml   -> zwei Ebenen hoch von gen/, dann in registry/
#   glx/libgl-symbols.txt   -> vier Ebenen hoch von gen/ (src/mesa/glapi/glapi/gen -> src/)
set(GL_AND_ES_API_XML ${GEN_ROOT}/gl_and_es_API.xml)
set(GL_API_XML ${GEN_ROOT}/gl_API.xml)
set(GL_XML ${GEN_ROOT}/../registry/gl.xml)
set(LIBGL_SYMBOLS_TXT ${GEN_ROOT}/../../../../glx/libgl-symbols.txt)

# glapi_xml_py_deps wird von src/mesa/glapi/CMakeLists.txt im Parent-Scope
# gesetzt (Liste aller .xml/.py Dateien, die die Codegen-Skripte indirekt
# lesen -> triggert Rebuilds).

# --- glapi_mapi_tmp.h ------------------------------------------------------
set(GLAPI_MAPI_TMP_H ${GEN_OUT_DIR}/glapi_mapi_tmp.h)
add_custom_command(
        OUTPUT ${GLAPI_MAPI_TMP_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/../../mapi_abi.py
        --printer glapi --gl_symbols ${LIBGL_SYMBOLS_TXT} ${GL_AND_ES_API_XML}
        > ${GLAPI_MAPI_TMP_H}
        DEPENDS ${GEN_ROOT}/../../mapi_abi.py ${GL_AND_ES_API_XML} ${LIBGL_SYMBOLS_TXT}
        ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating glapi_mapi_tmp.h"
        VERBATIM
)

# --- glapi_gentable.c -------------------------------------------------------
set(GLAPI_GENTABLE_C ${GEN_OUT_DIR}/glapi_gentable.c)
add_custom_command(
        OUTPUT ${GLAPI_GENTABLE_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/gl_gentable.py -f ${GL_AND_ES_API_XML}
        > ${GLAPI_GENTABLE_C}
        DEPENDS ${GEN_ROOT}/gl_gentable.py ${GL_AND_ES_API_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating glapi_gentable.c"
        VERBATIM
)

# --- enums.c -----------------------------------------------------------
set(MAIN_ENUMS_C ${GEN_OUT_DIR}/enums.c)
add_custom_command(
        OUTPUT ${MAIN_ENUMS_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/gl_enums.py -f ${GL_XML} > ${MAIN_ENUMS_C}
        DEPENDS ${GEN_ROOT}/gl_enums.py ${GL_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating mesa/main/enums.c"
        VERBATIM
)

# --- api_exec_init.c -----------------------------------------------------
set(MAIN_API_EXEC_C ${GEN_OUT_DIR}/api_exec_init.c)
add_custom_command(
        OUTPUT ${MAIN_API_EXEC_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/api_exec_init.py -f ${GL_AND_ES_API_XML}
        > ${MAIN_API_EXEC_C}
        DEPENDS ${GEN_ROOT}/api_exec_init.py ${GL_AND_ES_API_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating mesa/main/api_exec_init.c"
        VERBATIM
)

# --- api_exec_decl.h -----------------------------------------------------
set(MAIN_API_EXEC_DECL_H ${GEN_OUT_DIR}/api_exec_decl.h)
add_custom_command(
        OUTPUT ${MAIN_API_EXEC_DECL_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/api_exec_decl_h.py -f ${GL_AND_ES_API_XML}
        > ${MAIN_API_EXEC_DECL_H}
        DEPENDS ${GEN_ROOT}/api_exec_decl_h.py ${GL_AND_ES_API_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating mesa/main/api_exec_decl.h"
        VERBATIM
)

# --- api_save_init.h -----------------------------------------------------
set(MAIN_API_SAVE_INIT_H ${GEN_OUT_DIR}/api_save_init.h)
add_custom_command(
        OUTPUT ${MAIN_API_SAVE_INIT_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/api_save_init_h.py -f ${GL_AND_ES_API_XML}
        > ${MAIN_API_SAVE_INIT_H}
        DEPENDS ${GEN_ROOT}/api_save_init_h.py ${GL_AND_ES_API_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating mesa/main/api_save_init.h"
        VERBATIM
)

# --- api_save.h ----------------------------------------------------------
set(MAIN_API_SAVE_H ${GEN_OUT_DIR}/api_save.h)
add_custom_command(
        OUTPUT ${MAIN_API_SAVE_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/api_save_h.py -f ${GL_AND_ES_API_XML} > ${MAIN_API_SAVE_H}
        DEPENDS ${GEN_ROOT}/api_save_h.py ${GL_AND_ES_API_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating mesa/main/api_save.h"
        VERBATIM
)

# --- api_beginend_init.h --------------------------------------------------
set(MAIN_API_BEGINEND_INIT_H ${GEN_OUT_DIR}/api_beginend_init.h)
add_custom_command(
        OUTPUT ${MAIN_API_BEGINEND_INIT_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/api_beginend_init_h.py -f ${GL_AND_ES_API_XML}
        > ${MAIN_API_BEGINEND_INIT_H}
        DEPENDS ${GEN_ROOT}/api_beginend_init_h.py ${GL_AND_ES_API_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating mesa/main/api_beginend_init.h"
        VERBATIM
)

# --- api_hw_select_init.h --------------------------------------------------
set(MAIN_API_HW_SELECT_INIT_H ${GEN_OUT_DIR}/api_hw_select_init.h)
add_custom_command(
        OUTPUT ${MAIN_API_HW_SELECT_INIT_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/api_hw_select_init_h.py -f ${GL_API_XML}
        > ${MAIN_API_HW_SELECT_INIT_H}
        DEPENDS ${GEN_ROOT}/api_hw_select_init_h.py ${GL_API_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating mesa/main/api_hw_select_init.h"
        VERBATIM
)

# --- dispatch.h ----------------------------------------------------------
set(MAIN_DISPATCH_H ${GEN_OUT_DIR}/dispatch.h)
add_custom_command(
        OUTPUT ${MAIN_DISPATCH_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/gl_table.py -f ${GL_AND_ES_API_XML} -m dispatch
        > ${MAIN_DISPATCH_H}
        DEPENDS ${GEN_ROOT}/gl_table.py ${GL_AND_ES_API_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating mesa/main/dispatch.h"
        VERBATIM
)

# --- marshal_generated.h ---------------------------------------------------
set(MAIN_MARSHAL_GENERATED_H ${GEN_OUT_DIR}/marshal_generated.h)
add_custom_command(
        OUTPUT ${MAIN_MARSHAL_GENERATED_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/marshal_generated_h.py ${GL_AND_ES_API_XML}
        ${SIZEOF_POINTER} > ${MAIN_MARSHAL_GENERATED_H}
        DEPENDS ${GEN_ROOT}/marshal_generated_h.py ${GL_AND_ES_API_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating mesa/main/marshal_generated.h"
        VERBATIM
)

# --- unmarshal_table.c -----------------------------------------------------
set(MAIN_UNMARSHAL_TABLE_C ${GEN_OUT_DIR}/unmarshal_table.c)
add_custom_command(
        OUTPUT ${MAIN_UNMARSHAL_TABLE_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/unmarshal_table_c.py ${GL_AND_ES_API_XML}
        ${SIZEOF_POINTER} > ${MAIN_UNMARSHAL_TABLE_C}
        DEPENDS ${GEN_ROOT}/unmarshal_table_c.py ${GL_AND_ES_API_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating mesa/main/unmarshal_table.c"
        VERBATIM
)

# --- api_trace.c -----------------------------------------------------------
set(MAIN_API_TRACE_C ${GEN_OUT_DIR}/api_trace.c)
add_custom_command(
        OUTPUT ${MAIN_API_TRACE_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/api_trace_c.py -f ${GL_AND_ES_API_XML} > ${MAIN_API_TRACE_C}
        DEPENDS ${GEN_ROOT}/api_trace_c.py ${GL_AND_ES_API_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating mesa/main/api_trace.c"
        VERBATIM
)

# --- marshal_generated{0..7}.c ---------------------------------------------
# Meson's foreach ueber ['0'..'7'] -> acht parallele Shards, jeder mit
# Shard-Index und Shard-Anzahl (hier fest 8) als Argument.
set(MAIN_MARSHAL_GENERATED_C "")
foreach(shard RANGE 0 7)
    set(_marshal_shard_c ${GEN_OUT_DIR}/marshal_generated${shard}.c)
    add_custom_command(
            OUTPUT ${_marshal_shard_c}
            COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/marshal_generated_c.py ${GL_AND_ES_API_XML}
            ${shard} 8 ${SIZEOF_POINTER} > ${_marshal_shard_c}
            DEPENDS ${GEN_ROOT}/marshal_generated_c.py ${GL_AND_ES_API_XML} ${GLAPI_XML_PY_DEPS}
            COMMENT "Generating mesa/main/marshal_generated${shard}.c"
            VERBATIM
    )
    list(APPEND MAIN_MARSHAL_GENERATED_C ${_marshal_shard_c})
endforeach()

# --- GLX indirect rendering: indirect.c / indirect.h / indirect_init.c -----
# Meson: foreach x : [['indirect.c','proto'], ['indirect.h','init_h'], ['indirect_init.c','init_c']]
# CMake hat keine Liste-von-Listen, daher zwei parallele Listen statt
# verschachtelter list(GET)-Indirektion.
set(GLX_GENERATED "")
set(_glx_out_names indirect.c indirect.h indirect_init.c)
set(_glx_modes proto init_h init_c)

list(LENGTH _glx_out_names _glx_count)
math(EXPR _glx_last_index "${_glx_count} - 1")

foreach(_i RANGE 0 ${_glx_last_index})
    list(GET _glx_out_names ${_i} _glx_out)
    list(GET _glx_modes ${_i} _glx_mode)
    set(_glx_out_path ${GEN_OUT_DIR}/${_glx_out})
    add_custom_command(
            OUTPUT ${_glx_out_path}
            COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/glX_proto_send.py -f ${GL_API_XML}
            -m ${_glx_mode} -s ${LIBGL_SYMBOLS_TXT} > ${_glx_out_path}
            DEPENDS ${GEN_ROOT}/glX_proto_send.py ${GL_API_XML} ${LIBGL_SYMBOLS_TXT} ${GLAPI_XML_PY_DEPS}
            COMMENT "Generating glx/${_glx_out}"
            VERBATIM
    )
    list(APPEND GLX_GENERATED ${_glx_out_path})
endforeach()

# --- GLX indirect size: indirect_size.h / indirect_size.c ------------------
set(INDIRECT_SIZE_H ${GEN_OUT_DIR}/indirect_size.h)
add_custom_command(
        OUTPUT ${INDIRECT_SIZE_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/glX_proto_size.py -f ${GL_API_XML}
        --only-set -m size_h --header-tag _INDIRECT_SIZE_H_ > ${INDIRECT_SIZE_H}
        DEPENDS ${GEN_ROOT}/glX_proto_size.py ${GL_API_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating glx/indirect_size.h"
        VERBATIM
)

set(INDIRECT_SIZE_C ${GEN_OUT_DIR}/indirect_size.c)
add_custom_command(
        OUTPUT ${INDIRECT_SIZE_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_ROOT}/glX_proto_size.py -f ${GL_API_XML}
        --only-set -m size_c > ${INDIRECT_SIZE_C}
        DEPENDS ${GEN_ROOT}/glX_proto_size.py ${GL_API_XML} ${GLAPI_XML_PY_DEPS}
        COMMENT "Generating glx/indirect_size.c"
        VERBATIM
)

list(APPEND GLX_GENERATED ${INDIRECT_SIZE_H} ${INDIRECT_SIZE_C})
set(GLX_INDIRECT_SIZE_H ${INDIRECT_SIZE_H})

set(ALL_GLAPI_GEN_FILES
        ${GLAPI_MAPI_TMP_H}
        ${GLAPI_GENTABLE_C}
        ${MAIN_ENUMS_C}
        ${MAIN_API_EXEC_C}
        ${MAIN_API_EXEC_DECL_H}
        ${MAIN_API_SAVE_INIT_H}
        ${MAIN_API_SAVE_H}
        ${MAIN_API_BEGINEND_INIT_H}
        ${MAIN_API_HW_SELECT_INIT_H}
        ${MAIN_DISPATCH_H}
        ${MAIN_MARSHAL_GENERATED_H}
        ${MAIN_UNMARSHAL_TABLE_C}
        ${MAIN_API_TRACE_C}
        ${MAIN_MARSHAL_GENERATED_C}
        ${GLX_GENERATED}
)
set_source_files_properties(${ALL_GLAPI_GEN_FILES} PROPERTIES GENERATED TRUE)

add_custom_target(glapi_gen DEPENDS ${ALL_GLAPI_GEN_FILES})

# --- Alles an src/mesa/glapi/glapi/CMakeLists.txt (Parent-Scope) reichen ---
set(GLAPI_MAPI_TMP_H ${GLAPI_MAPI_TMP_H} PARENT_SCOPE)
set(GLAPI_GENTABLE_C ${GLAPI_GENTABLE_C} PARENT_SCOPE)
set(MAIN_ENUMS_C ${MAIN_ENUMS_C} PARENT_SCOPE)
set(MAIN_API_EXEC_C ${MAIN_API_EXEC_C} PARENT_SCOPE)
set(MAIN_API_EXEC_DECL_H ${MAIN_API_EXEC_DECL_H} PARENT_SCOPE)
set(MAIN_API_SAVE_INIT_H ${MAIN_API_SAVE_INIT_H} PARENT_SCOPE)
set(MAIN_API_SAVE_H ${MAIN_API_SAVE_H} PARENT_SCOPE)
set(MAIN_API_BEGINEND_INIT_H ${MAIN_API_BEGINEND_INIT_H} PARENT_SCOPE)
set(MAIN_API_HW_SELECT_INIT_H ${MAIN_API_HW_SELECT_INIT_H} PARENT_SCOPE)
set(MAIN_DISPATCH_H ${MAIN_DISPATCH_H} PARENT_SCOPE)
set(MAIN_MARSHAL_GENERATED_H ${MAIN_MARSHAL_GENERATED_H} PARENT_SCOPE)
set(MAIN_UNMARSHAL_TABLE_C ${MAIN_UNMARSHAL_TABLE_C} PARENT_SCOPE)
set(MAIN_API_TRACE_C ${MAIN_API_TRACE_C} PARENT_SCOPE)
set(MAIN_MARSHAL_GENERATED_C ${MAIN_MARSHAL_GENERATED_C} PARENT_SCOPE)
set(GLX_GENERATED ${GLX_GENERATED} PARENT_SCOPE)
set(GLX_INDIRECT_SIZE_H ${GLX_INDIRECT_SIZE_H} PARENT_SCOPE)
set(GEN_OUT_DIR ${GEN_OUT_DIR} PARENT_SCOPE)