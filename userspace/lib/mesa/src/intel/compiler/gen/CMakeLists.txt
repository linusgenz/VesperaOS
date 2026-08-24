# lib/mesa/src/intel/compiler/gen/CMakeLists.txt
#
# Entspricht Mesa upstream src/intel/compiler/gen/meson.build.
# Reiner Codegen + Encoding/Decoding-Layer fuer Intel-Instruktionsformate,
# generiert aus gen_opcodes.py + JSON-Beschreibungen (pre_xe.json / xe.json).
# Wir brauchen nur pre_xe.json fuer Gen9 (Xe/Xe2/Xe3 sind fuer euch irrelevant,
# das ist reines Gen12+ Namensschema von Intel), generieren aber trotzdem
# alle vier Header wie upstream, da gen_inst_info.py sie in einem Aufruf
# gemeinsam erzeugt -- das selektive Weglassen einzelner Outputs wuerde nur
# unnoetige Komplexitaet in den Custom-Command einbauen.
#
# with_tests und with_intel_tools (gentool) sind bei uns aus -- keine
# gtest-Suite, kein gentool-Debug-Tool.

set(GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(GEN_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR})

# --- gen_opcodes: gen_opcodes.h, gen_opcodes_private.h, gen_opcodes.cpp ---
set(GEN_OPCODES_H ${GEN_GENERATED_DIR}/gen_opcodes.h)
set(GEN_OPCODES_PRIVATE_H ${GEN_GENERATED_DIR}/gen_opcodes_private.h)
set(GEN_OPCODES_CPP ${GEN_GENERATED_DIR}/gen_opcodes.cpp)

add_custom_command(
        OUTPUT ${GEN_OPCODES_H} ${GEN_OPCODES_PRIVATE_H} ${GEN_OPCODES_CPP}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_DIR}/gen_opcodes_gen.py
        --public-header ${GEN_OPCODES_H}
        --private-header ${GEN_OPCODES_PRIVATE_H}
        --code ${GEN_OPCODES_CPP}
        DEPENDS ${GEN_DIR}/gen_opcodes_gen.py ${GEN_DIR}/gen_opcodes.py
        COMMENT "Generating gen_opcodes.{h,cpp}"
        VERBATIM
)

# --- gen_info_*.h: pre_xe, xe, xe2, xe3p Instruktionsinfo-Tabellen ---
set(GEN_INFO_PRE_XE_H ${GEN_GENERATED_DIR}/gen_info_pre_xe.h)
set(GEN_INFO_XE_H ${GEN_GENERATED_DIR}/gen_info_xe.h)
set(GEN_INFO_XE2_H ${GEN_GENERATED_DIR}/gen_info_xe2.h)
set(GEN_INFO_XE3P_H ${GEN_GENERATED_DIR}/gen_info_xe3p.h)

add_custom_command(
        OUTPUT ${GEN_INFO_PRE_XE_H} ${GEN_INFO_XE_H} ${GEN_INFO_XE2_H} ${GEN_INFO_XE3P_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${GEN_DIR}/gen_inst_info.py
        --json=${GEN_DIR}/pre_xe.json
        --json=${GEN_DIR}/xe.json
        --output-dir=${GEN_GENERATED_DIR}
        --cpp
        DEPENDS ${GEN_DIR}/gen_inst_info.py ${GEN_DIR}/pre_xe.json ${GEN_DIR}/xe.json ${GEN_DIR}/gen_opcodes.py
        COMMENT "Generating gen_info_*.h"
        VERBATIM
)

set(MESA_INTEL_COMPILER_GEN_SOURCES
        gen_names.c
        gen_compact.cpp
        gen_control_flow.cpp
        gen_parse.cpp
        gen_print.cpp
        gen_encoding.cpp
        gen_encoding_pre_xe.cpp
        gen_validate.cpp

        ${GEN_OPCODES_CPP}
        ${GEN_INFO_PRE_XE_H}
        ${GEN_INFO_XE_H}
        ${GEN_INFO_XE2_H}
        ${GEN_INFO_XE3P_H}
)

add_library(mesa_intel_compiler_gen STATIC ${MESA_INTEL_COMPILER_GEN_SOURCES})

target_include_directories(mesa_intel_compiler_gen
        PUBLIC
        ${GEN_DIR}
        ${GEN_GENERATED_DIR}
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_link_libraries(mesa_intel_compiler_gen
        PUBLIC
        mesa_util
        mesa_intel_dev
)

target_compile_options(mesa_intel_compiler_gen PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_CXX_FLAGS}>
)

add_dependencies(mesa_intel_compiler_gen sysroot_headers)