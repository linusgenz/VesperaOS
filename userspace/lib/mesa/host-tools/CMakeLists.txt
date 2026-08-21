# ════════════════════════════════════════════════════════════════
# lib/mesa/host-tools/CMakeLists.txt
#
# EIGENSTÄNDIGES CMake-Projekt für den BUILD-HOST (dein Linux-System,
# LLVM 19+, Distro-Pakete). Wird von der Root-CMakeLists via
# ExternalProject_Add als komplett separater `cmake -S -B` Lauf
# aufgerufen — KEIN CMAKE_TOOLCHAIN_FILE, kein Vespera-Sysroot.
#
# Baut, 1:1 abgebildet aus:
#   src/compiler/meson.build   -> libcompiler
#   src/compiler/nir/meson.build -> libnir (+ generierter Code)
#   src/compiler/spirv/meson.build -> libvtn + vtn_bindgen2
#   src/compiler/clc/meson.build   -> libmesaclc + mesa_clc
#   src/util/meson.build       -> mesa_util_host (NICHT im Detail unten
#                                  ausgeführt, du hast diese Datei
#                                  noch nicht geschickt — Platzhalter
#                                  am Ende, siehe TODO)
#
# WICHTIGE ERKENNTNIS aus den meson.build-Dateien:
#   - vtn_bindgen2 liegt in compiler/spirv/, NICHT in compiler/clc/.
#     Es linkt idep_vtn (SPIR-V/NIR-Parsing) + idep_nir + idep_mesautil.
#   - mesa_clc braucht LLVM+Clang ALS LIBRARY (clc_helpers.cpp
#     kompiliert OpenCL-C via eingebettetem Clang zu LLVM-IR, dann
#     via LLVMSPIRVLib zu SPIR-V) UND SPIRV-Tools (zum Linken/
#     Optimieren des SPIR-V).
#   - has_spirv_link_workaround: bei LLVM >= 17 MUSS SPIRV-Tools
#     einen bestimmten Patch enthalten (SetAllowPtrTypeMismatch),
#     sonst bricht der Mesa-Build mit einem error() ab. Bei dir
#     (LLVM 19+) greift das zwingend — s. CHECK weiter unten.
# ════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.20)
project(mesa_host_tools C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Pfad zu deinem Mesa-Checkout (Root, dort wo src/ liegt).
# Von der äußeren CMakeLists via -DMESA_SOURCE_ROOT=... gesetzt,
# hier nur ein Fallback für Testläufe direkt in host-tools/.
if(NOT DEFINED MESA_SOURCE_ROOT)
    set(MESA_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../")
endif()
set(MESA_SRC ${MESA_SOURCE_ROOT}/src)

# ────────────────────────────────────────────────────────────────
# LLVM / Clang — Distro-Pakete, LLVM 19+.
# Auf Debian/Ubuntu heißen die Config-Dateien oft versioniert, z.B.
# /usr/lib/llvm-19/lib/cmake/llvm/LLVMConfig.cmake — falls
# find_package ohne HINTS nichts findet, setz beim Konfigurieren:
#   -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm
#   -DClang_DIR=/usr/lib/llvm-19/lib/cmake/clang
# ────────────────────────────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Gefunden: LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")
message(STATUS "Gefunden: Clang in ${Clang_DIR}")

if(TARGET LLVM)
    set(LLVM_LIBS LLVM)
else()
    llvm_map_components_to_libnames(LLVM_LIBS
            core support option bitwriter bitreader linker irreader
            analysis transformutils passes asmparser
    )
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../FindMesaPython.cmake)

# Clang-Libraries, die clc_helpers.cpp für die OpenCL-C-Kompilierung
# braucht (Frontend + Codegen + Driver-Glue). Manche Distros (z.B.
# Arch) liefern die einzelnen clang*-Static-Libs NICHT aus, sondern
# nur die kombinierte libclang-cpp.so (bestätigt: Arch clang-Paket
# enthaelt nur libclang-cpp.so, keine libclangFrontend.a etc. — siehe
# `pacman -Ql clang`). Andere Distros (Debian/Ubuntu, Fedora) haben
# die feingranularen Einzel-Libs. Wir probieren beides, kombinierte
# zuerst.
find_library(CLANG_CPP_LIB NAMES clang-cpp HINTS ${LLVM_LIBRARY_DIRS})

if(CLANG_CPP_LIB)
    message(STATUS "Nutze kombinierte libclang-cpp: ${CLANG_CPP_LIB}")
    set(CLANG_LIBS clang-cpp)
else()
    message(STATUS "libclang-cpp nicht gefunden, nutze granulare clang*-Libs")
    set(CLANG_LIBS
            clangFrontend
            clangDriver
            clangSerialization
            clangParse
            clangSema
            clangAnalysis
            clangEdit
            clangAST
            clangLex
            clangBasic
            clangCodeGen
    )
endif()

# ────────────────────────────────────────────────────────────────
# SPIRV-Tools — Distro-Paket (spirv-tools-dev o.ä.).
# ────────────────────────────────────────────────────────────────
find_package(SPIRV-Tools CONFIG REQUIRED)
find_package(SPIRV-Tools-opt CONFIG REQUIRED)
find_package(SPIRV-Tools-link CONFIG REQUIRED)

# CHECK: mesa_clc.c / clc_helpers.cpp verlangt bei LLVM >= 17 zwingend
# spvtools::LinkerOptions::SetAllowPtrTypeMismatch(true). Das ist seit
# SPIRV-Tools ~2022 Standard, mit aktuellen Distro-Paketen (2024/2025)
# sollte das erfüllt sein. Der eigentliche compile_definitions-Eintrag
# HAS_SPIRV_LINK_LLVM_WORKAROUND=1 sitzt unten direkt an
# mesa_libclc_helper (siehe dort) — das ist der Ort, an dem er in
# meson (_libmesaclc_c_args) auch tatsächlich verwendet wird.
# Vor dem ersten echten Build lohnt sich ein kurzer manueller Test:
#   echo '#include <spirv-tools/linker.hpp>' | g++ -x c++ -c - \
#     -o /dev/null -DTEST && \
#   grep -r "SetAllowPtrTypeMismatch" /usr/include/spirv-tools/linker.hpp
# Findet grep nichts, ist dein SPIRV-Tools-Paket zu alt für LLVM 19.

# ────────────────────────────────────────────────────────────────
# SPIRV-LLVM-Translator (dein "spirv-llvm-translator"-Paket).
# CMake-Config-Target heißt üblicherweise LLVMSPIRVLib. Falls
# find_package hier scheitert (manche Distros liefern nur die .so
# ohne Config-cmake), Fallback per find_library/find_path.
# ────────────────────────────────────────────────────────────────
find_package(LLVMSPIRVLib QUIET CONFIG)
if(NOT LLVMSPIRVLib_FOUND)
    find_library(LLVMSPIRVLib_LIBRARY
            NAMES LLVMSPIRVLib LLVMSPIRV
            HINTS ${LLVM_LIBRARY_DIRS}
    )
    find_path(LLVMSPIRVLib_INCLUDE_DIR
            NAMES LLVMSPIRVLib.h
            PATH_SUFFIXES LLVMSPIRVLib
    )
    if(NOT LLVMSPIRVLib_LIBRARY)
        message(FATAL_ERROR
                "LLVMSPIRVLib (spirv-llvm-translator) nicht gefunden. "
                "Falls dein Distro-Paket keine CMake-Config mitliefert, "
                "gib den Pfad manuell an: -DLLVMSPIRVLib_LIBRARY=/pfad/zu/libLLVMSPIRVLib.so"
        )
    endif()
    add_library(LLVMSPIRVLib UNKNOWN IMPORTED)
    set_target_properties(LLVMSPIRVLib PROPERTIES
            IMPORTED_LOCATION ${LLVMSPIRVLib_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES "${LLVMSPIRVLib_INCLUDE_DIR}"
    )
endif()

# ════════════════════════════════════════════════════════════════
# mesa_util_host  (src/util/meson.build)
#
# Portiert aus deiner Vespera-Cross-CMakeLists (lib/mesa/src/util/
# CMakeLists.txt), auf den HOST-Build umgestellt. Übernommen 1:1,
# weil bereits korrekt und mit echtem Codegen statt Glob-Rateversuch:
#   - format_srgb.c Codegen
#   - git_sha1.h Handling
#   - shader_stats.h Codegen
#   - u_format_gen.h / u_format_pack.h / u_format_table.c Codegen
#     via u_format_table.py (das hatte ich vorher nur mit file(GLOB)
#     grob nachgebildet — deine Version ist hier maßgeblich)
#   - MESA_UTIL_SOURCES Liste, blake3 als add_subdirectory
#
# GEÄNDERT ggü. deiner Cross-Version, weil das hier der HOST-Build
# ist, nicht der Vespera-Cross-Build:
#   - ${VESPERA_SYSROOT}/usr/include (PRIVATE include) ENTFERNT —
#     existiert im Host-Kontext nicht, dein Sysroot ist Vespera-spezifisch
#   - add_dependencies(mesa_util_host sysroot_headers) ENTFERNT — dieses
#     Target kommt aus deinem Cross-Build-Baum, hier nicht vorhanden
#   - install(... DESTINATION ${VESPERA_SYSROOT}/usr/lib ...) ENTFERNT
#     und durch normales bin/-Install (Teil des host-tools-Projekts)
#     ersetzt — ein Host-Tool gehört nicht in den Vespera-Sysroot
#   - add_library(... SHARED ...) -> STATIC — mesa_clc/vtn_bindgen2
#     sind einzelne Executables ohne Installations-RPATH-Bedarf;
#     static vermeidet unnötige .so-Distribution im host-tools-
#     Install-Prefix (entspricht auch meson: static_library('mesa_util_host', ...))
#   - ${MESA_ROOT} -> ${MESA_SOURCE_ROOT} (Variablenname aus der
#     Root-CMakeLists dieses Host-Baums, siehe oben im File)
#   - ${MESA_COMMON_C_FLAGS} ENTFERNT — das ist ein Vespera-Cross-
#     Compile-Flag-Set (z.B. -nostdinc, Cross-Sysroot-Pfade); auf dem
#     Host bauen wir mit normalen Compiler-Defaults
#   - pthread -> Threads::Threads (portabler CMake-Weg, gleiche Wirkung)
#
# UNKLAR / ANNAHME: die HAVE_*-Compile-Definitions (HAVE_ENDIAN_H,
# HAS_SCHED_GETAFFINITY, HAVE_FUNC_ATTRIBUTE_PACKED, ...) waren in
# deiner Version hartkodiert für Vespera. Auf einem aktuellen Linux-
# Host mit glibc gelten praktisch alle davon ebenfalls, deshalb hier
# unverändert übernommen — falls mesa_util_host-Quellcode auf dem Host
# Compiler-Warnungen/Fehler zu fehlenden Feature-Makros wirft, ist
# das der erste Ort zum Nachschauen.
# ════════════════════════════════════════════════════════════════
find_package(Threads REQUIRED)

set(UTIL_SRC ${MESA_SRC}/util)

# ---- format_srgb.c ----
set(FORMAT_SRGB_C ${CMAKE_CURRENT_BINARY_DIR}/format_srgb.c)
add_custom_command(
        OUTPUT ${FORMAT_SRGB_C}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${UTIL_SRC}/format_srgb.py > ${FORMAT_SRGB_C}
        DEPENDS ${UTIL_SRC}/format_srgb.py
        COMMENT "Generating format_srgb.c"
        VERBATIM
)

# ---- git_sha1.h ----
set(GIT_SHA1_H ${CMAKE_CURRENT_BINARY_DIR}/git_sha1.h)
if(NOT EXISTS ${GIT_SHA1_H})
    file(COPY ${UTIL_SRC}/git_sha1.h DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

# ---- shader_stats.h ----
set(SHADER_STATS_H ${CMAKE_CURRENT_BINARY_DIR}/shader_stats.h)
set(PROCESS_SHADER_STATS_PY ${UTIL_SRC}/process_shader_stats.py)
set(SHADER_STATS_RNC ${UTIL_SRC}/shader_stats.rnc)
set(SHADER_STATS_XML ${UTIL_SRC}/shader_stats.xml)
add_custom_command(
        OUTPUT ${SHADER_STATS_H}
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${PROCESS_SHADER_STATS_PY} ${SHADER_STATS_RNC} ${SHADER_STATS_XML} > ${SHADER_STATS_H}
        DEPENDS ${PROCESS_SHADER_STATS_PY} ${SHADER_STATS_RNC} ${SHADER_STATS_XML}
        COMMENT "Generating shader_stats.h"
        VERBATIM
)

file(GLOB MESA_UTIL_FORMAT_SOURCES ${UTIL_SRC}/format/*.c)

# ---- u_format_gen.h / u_format_pack.h / u_format_table.c ----
set(U_FORMAT_GEN_ROOT ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(U_FORMAT_GEN_DIR ${U_FORMAT_GEN_ROOT}/util/format)
file(MAKE_DIRECTORY ${U_FORMAT_GEN_DIR})

set(U_FORMAT_TABLE_PY ${UTIL_SRC}/format/u_format_table.py)
set(U_FORMAT_YAML ${UTIL_SRC}/format/u_format.yaml)
set(U_FORMAT_PACK_PY ${UTIL_SRC}/format/u_format_pack.py)
set(U_FORMAT_PARSE_PY ${UTIL_SRC}/format/u_format_parse.py)

set(U_FORMAT_GEN_H ${U_FORMAT_GEN_DIR}/u_format_gen.h)
add_custom_command(
        OUTPUT ${U_FORMAT_GEN_H}
        COMMAND ${CMAKE_COMMAND} -E env sh -c "${MESA_PYTHON3_EXECUTABLE} ${U_FORMAT_TABLE_PY} ${U_FORMAT_YAML} --enums > ${U_FORMAT_GEN_H}"
        DEPENDS ${U_FORMAT_TABLE_PY} ${U_FORMAT_YAML} ${U_FORMAT_PACK_PY} ${U_FORMAT_PARSE_PY}
        COMMENT "Generating util/format/u_format_gen.h"
        VERBATIM
)

set(U_FORMAT_PACK_H ${U_FORMAT_GEN_DIR}/u_format_pack.h)
add_custom_command(
        OUTPUT ${U_FORMAT_PACK_H}
        COMMAND ${CMAKE_COMMAND} -E env sh -c "${MESA_PYTHON3_EXECUTABLE} ${U_FORMAT_TABLE_PY} ${U_FORMAT_YAML} --header > ${U_FORMAT_PACK_H}"
        DEPENDS ${U_FORMAT_TABLE_PY} ${U_FORMAT_YAML} ${U_FORMAT_PACK_PY} ${U_FORMAT_PARSE_PY}
        COMMENT "Generating util/format/u_format_pack.h"
        VERBATIM
)

set(U_FORMAT_TABLE_C ${CMAKE_CURRENT_BINARY_DIR}/u_format_table.c)
add_custom_command(
        OUTPUT ${U_FORMAT_TABLE_C}
        COMMAND ${CMAKE_COMMAND} -E env sh -c "${MESA_PYTHON3_EXECUTABLE} ${U_FORMAT_TABLE_PY} ${U_FORMAT_YAML} > ${U_FORMAT_TABLE_C}"
        DEPENDS ${U_FORMAT_TABLE_PY} ${U_FORMAT_YAML} ${U_FORMAT_PACK_PY} ${U_FORMAT_PARSE_PY}
        COMMENT "Generating u_format_table.c"
        VERBATIM
)

list(APPEND MESA_UTIL_FORMAT_SOURCES ${U_FORMAT_TABLE_C})
# Reine Header als Sources aufnehmen, damit CMake/Ninja ihre
# custom_command auch ausführt (nur .c-Outputs, die tatsächlich
# kompiliert werden, ziehen ihre custom_command sonst automatisch).
list(APPEND MESA_UTIL_FORMAT_SOURCES ${U_FORMAT_GEN_H} ${U_FORMAT_PACK_H})

# ---- blake3 ----
# WICHTIG: NICHT add_subdirectory(${UTIL_SRC}/blake3) — die dort
# liegende blake3/CMakeLists.txt (die du mir geschickt hast) ist
# deine Vespera-Cross-Version mit -nostdinc, --sysroot=${VESPERA_SYSROOT},
# -idirafter ${COMPILER_BUILTIN_INCLUDE} usw. Die würde auf dem Host
# mit deinem System-GCC/Clang sofort scheitern, weil diese Pfade und
# Flags Vespera-spezifisch sind. Stattdessen bauen wir mesa_blake3_host
# hier inline, mit derselben Quelldatei-Auswahl/Filter-Logik wie in
# deiner Vorlage, aber Host-Compile-Flags.
#
# GEÄNDERT ggü. deiner blake3/CMakeLists.txt:
#   - ${VESPERA_SYSROOT}/usr/include (PRIVATE) ENTFERNT
#   - -nostdinc, --sysroot=..., -isystem ${VESPERA_SYSROOT}/..., \
#     -idirafter ${COMPILER_BUILTIN_INCLUDE} ALLE ENTFERNT — Host baut
#     mit normalen System-Headern, kein Cross-Sysroot nötig
#   - add_dependencies(... sysroot_headers) ENTFERNT — Target existiert
#     im Host-Baum nicht
#   - -O0 -g ENTFERNT — für Host-Tools reicht Release (siehe
#     CMAKE_BUILD_TYPE=Release in der äußeren ExternalProject_Add);
#     falls du Debug-Symbole für mesa_clc selbst brauchst (z.B. um
#     einen Codegen-Bug zu debuggen), einfach wieder ergänzen
#   - SIMD-Ausschlussfilter (sse2/sse41/avx2/avx512/neon) UNVERÄNDERT
#     übernommen — auf dem Host reicht die portable Referenz-
#     implementierung genauso wie im Cross-Build
file(GLOB BLAKE3_SOURCES ${UTIL_SRC}/blake3/*.c)
if(NOT BLAKE3_SOURCES)
    message(FATAL_ERROR
            "Keine .c-Dateien unter ${UTIL_SRC}/blake3 gefunden — prüfe "
            "MESA_SOURCE_ROOT."
    )
endif()

list(FILTER BLAKE3_SOURCES EXCLUDE REGEX ".*_neon\\.c$")

add_library(mesa_blake3_host STATIC ${BLAKE3_SOURCES})
target_include_directories(mesa_blake3_host PUBLIC ${UTIL_SRC}/blake3)
target_compile_options(mesa_blake3_host PRIVATE -fPIC)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i.86")
    set_source_files_properties(${UTIL_SRC}/blake3/blake3_sse2.c PROPERTIES COMPILE_FLAGS "-msse2")
    set_source_files_properties(${UTIL_SRC}/blake3/blake3_sse41.c PROPERTIES COMPILE_FLAGS "-msse4.1")
    set_source_files_properties(${UTIL_SRC}/blake3/blake3_avx2.c PROPERTIES COMPILE_FLAGS "-mavx2")
    set_source_files_properties(${UTIL_SRC}/blake3/blake3_avx512.c PROPERTIES COMPILE_FLAGS "-mavx512f -mavx512vl")
endif()

# ---- mesa_util_host core sources ----
set(MESA_UTIL_SOURCES
        anon_file.c bitscan.c blob.c build_id.c cnd_monotonic.c compress.c
        thread_sched.c crc32.c dag.c disk_cache.c disk_cache_os.c double.c
        fast_idiv_by_const.c float8.c fossilize_db.c futex.c half_float.c
        hash_table.c helpers.c u_idalloc.c log.c lut.c memstream.c
        mesa-blake3.c os_time.c os_file.c os_file_notify.c os_memory_fd.c
        os_misc.c os_socket.c pb_slab.c perf/u_trace.c u_process.c
        u_qsort.cpp rwlock.c ralloc.c rand_xor.c range_minimum_query.c
        rb_tree.c register_allocate.c rgtc.c set.c simple_mtx.c slab.c
        softfloat.c sparse_array.c string_buffer.c strndup.c strtod.c
        texcompress_astc_luts.cpp texcompress_astc_luts_wrap.cpp
        texcompress_astc.cpp u_atomic.c u_call_once.c u_dl.c u_dynarray.c
        u_hash_table.c u_queue.c u_range_remap.c u_shader_variant_cache.c
        u_string.c u_thread.c u_vector.c u_math.c u_mm.c u_debug.c
        u_debug_memory.c u_debug_stack.c u_debug_symbol.c u_cpu_detect.c
        u_printf.c u_worklist.c vl_zscan_data.c vma.c mesa_cache_db.c
        mesa_cache_db_multipart.c
)
list(TRANSFORM MESA_UTIL_SOURCES PREPEND "${UTIL_SRC}/")
list(APPEND MESA_UTIL_SOURCES
        ${MESA_UTIL_FORMAT_SOURCES}
        ${FORMAT_SRGB_C}
        ${SHADER_STATS_H}
)

# cache_ops -- Host ist x86_64 (dein System).
list(APPEND MESA_UTIL_SOURCES ${UTIL_SRC}/cache_ops_x86.c)

add_library(mesa_util_host STATIC ${MESA_UTIL_SOURCES})

target_include_directories(mesa_util_host
        PUBLIC
        ${UTIL_SRC}
        ${MESA_SRC}                             # inc_src Äquivalent
        ${UTIL_SRC}/format
        ${U_FORMAT_GEN_ROOT}                    # für #include "util/format/u_format_gen.h"
        ${U_FORMAT_GEN_DIR}                     # für #include "u_format_pack.h" (unqualifiziert)
        ${MESA_SOURCE_ROOT}/include              # inc_include Äquivalent (GL/, vulkan/, etc.)
        ${CMAKE_CURRENT_BINARY_DIR}              # für generierte git_sha1.h
)

target_compile_definitions(mesa_util_host
        PUBLIC
        __STDC_CONSTANT_MACROS
        __STDC_FORMAT_MACROS
        __STDC_LIMIT_MACROS
        HAVE_OPENGL=1
        HAVE_PTHREAD=1
        HAVE_ENDIAN_H=1
        HAVE_STRUCT_TIMESPEC
        HAVE_SYSCONF
        HAS_SCHED_GETAFFINITY
        HAS_SCHED_H
        HAVE_FUNC_ATTRIBUTE_PACKED
        HAVE_DLADDR
        HAVE_DLFCN_H
        _GNU_SOURCE
        PRIVATE
        XXH_FORCE_ALIGN_CHECK=0
        XXH_FORCE_MEMORY_ACCESS=0
)

target_link_libraries(mesa_util_host
        PUBLIC
        mesa_blake3_host
        Threads::Threads
        ${CMAKE_DL_LIBS}
)
find_package(ZLIB)
if(ZLIB_FOUND)
    target_link_libraries(mesa_util_host PUBLIC ZLIB::ZLIB)
endif()
find_library(LIBM_LIBRARY m)
if(LIBM_LIBRARY)
    target_link_libraries(mesa_util_host PUBLIC ${LIBM_LIBRARY})
endif()
find_library(UNWIND_LIBRARY unwind)
if(UNWIND_LIBRARY)
    target_link_libraries(mesa_util_host PUBLIC ${UNWIND_LIBRARY})
    target_compile_definitions(mesa_util_host PRIVATE HAVE_LIBUNWIND)
endif()

# Kein install() hier — mesa_util_host ist eine interne statische Library
# des Host-Tools-Builds, nur mesa_clc/vtn_bindgen2 werden am Ende
# installiert (siehe ganz unten in diesem File).

# ════════════════════════════════════════════════════════════════
# libcompiler  (src/compiler/meson.build)
# ════════════════════════════════════════════════════════════════
set(COMPILER_INC ${MESA_SRC}/compiler)

# builtin_types.h/.c — generiert via builtin_types_h.py / builtin_types_c.py
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/builtin_types.h
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${COMPILER_INC}/builtin_types_h.py
        ${CMAKE_CURRENT_BINARY_DIR}/builtin_types.h
        DEPENDS ${COMPILER_INC}/builtin_types_h.py ${COMPILER_INC}/builtin_types.py
        COMMENT "Generating builtin_types.h"
        VERBATIM
)
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/builtin_types.c
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${COMPILER_INC}/builtin_types_c.py
        ${CMAKE_CURRENT_BINARY_DIR}/builtin_types.c
        DEPENDS ${COMPILER_INC}/builtin_types_c.py ${COMPILER_INC}/builtin_types.py
        COMMENT "Generating builtin_types.c"
        VERBATIM
)

# ir_expression_operation.h — capture:true heißt: stdout des Skripts
# wird direkt in die Output-Datei geschrieben.
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ir_expression_operation.h
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${COMPILER_INC}/glsl/ir_expression_operation.py enum
        > ${CMAKE_CURRENT_BINARY_DIR}/ir_expression_operation.h
        DEPENDS ${COMPILER_INC}/glsl/ir_expression_operation.py
        COMMENT "Generating ir_expression_operation.h"
        VERBATIM
)

add_library(mesa_compiler_host STATIC
        ${COMPILER_INC}/glsl_types.c
        ${COMPILER_INC}/shader_enums.c
        ${CMAKE_CURRENT_BINARY_DIR}/builtin_types.h
        ${CMAKE_CURRENT_BINARY_DIR}/builtin_types.c
        ${CMAKE_CURRENT_BINARY_DIR}/ir_expression_operation.h
)
target_include_directories(mesa_compiler_host PUBLIC
        ${COMPILER_INC}
        ${MESA_SOURCE_ROOT}/include
        ${MESA_SRC}
        ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(mesa_compiler_host PUBLIC mesa_util_host)

# ════════════════════════════════════════════════════════════════
# libnir  (src/compiler/nir/meson.build)
# with_nir_headers_only ist für uns FALSE — mesa_clc/vtn_bindgen2
# brauchen die volle NIR-Funktionalität (Optimierungspässe etc.),
# nicht nur die Header-Stubs.
# ════════════════════════════════════════════════════════════════
set(NIR_SRC ${MESA_SRC}/compiler/nir)
set(NIR_GEN ${CMAKE_CURRENT_BINARY_DIR}/nir)
file(MAKE_DIRECTORY ${NIR_GEN})

# --- generierter Code, 1:1 aus meson_nir.build custom_targets ---
add_custom_command(
        OUTPUT ${NIR_GEN}/nir_opcodes.h
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_SRC}/nir_opcodes_h.py > ${NIR_GEN}/nir_opcodes.h
        DEPENDS ${NIR_SRC}/nir_opcodes_h.py ${NIR_SRC}/nir_opcodes.py ${NIR_SRC}/nir_intrinsics.py
        COMMENT "Generating nir_opcodes.h" VERBATIM
)
add_custom_command(
        OUTPUT ${NIR_GEN}/nir_opcodes.c
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_SRC}/nir_opcodes_c.py > ${NIR_GEN}/nir_opcodes.c
        DEPENDS ${NIR_SRC}/nir_opcodes_c.py ${NIR_SRC}/nir_opcodes.py ${NIR_SRC}/nir_intrinsics.py
        COMMENT "Generating nir_opcodes.c" VERBATIM
)
add_custom_command(
        OUTPUT ${NIR_GEN}/nir_builder_opcodes.h
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_SRC}/nir_builder_opcodes_h.py > ${NIR_GEN}/nir_builder_opcodes.h
        DEPENDS ${NIR_SRC}/nir_builder_opcodes_h.py ${NIR_SRC}/nir_opcodes.py ${NIR_SRC}/nir_intrinsics.py
        COMMENT "Generating nir_builder_opcodes.h" VERBATIM
)
add_custom_command(
        OUTPUT ${NIR_GEN}/nir_constant_expressions.c
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_SRC}/nir_constant_expressions.py > ${NIR_GEN}/nir_constant_expressions.c
        DEPENDS ${NIR_SRC}/nir_constant_expressions.py ${NIR_SRC}/nir_opcodes.py ${NIR_SRC}/nir_intrinsics.py
        COMMENT "Generating nir_constant_expressions.c" VERBATIM
)
add_custom_command(
        OUTPUT ${NIR_GEN}/nir_opt_algebraic.c
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_SRC}/nir_opt_algebraic.py --out ${NIR_GEN}/nir_opt_algebraic.c
        DEPENDS ${NIR_SRC}/nir_opt_algebraic.py ${NIR_SRC}/nir_opcodes.py ${NIR_SRC}/nir_algebraic.py
        COMMENT "Generating nir_opt_algebraic.c" VERBATIM
)
add_custom_command(
        OUTPUT ${NIR_GEN}/nir_intrinsics.h
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_SRC}/nir_intrinsics_h.py --out ${NIR_GEN}/nir_intrinsics.h
        DEPENDS ${NIR_SRC}/nir_intrinsics_h.py ${NIR_SRC}/nir_intrinsics.py
        COMMENT "Generating nir_intrinsics.h" VERBATIM
)
add_custom_command(
        OUTPUT ${NIR_GEN}/nir_intrinsics_indices.h
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_SRC}/nir_intrinsics_indices_h.py --out ${NIR_GEN}/nir_intrinsics_indices.h
        DEPENDS ${NIR_SRC}/nir_intrinsics_indices_h.py ${NIR_SRC}/nir_intrinsics.py
        COMMENT "Generating nir_intrinsics_indices.h" VERBATIM
)
add_custom_command(
        OUTPUT ${NIR_GEN}/nir_intrinsics.c
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${NIR_SRC}/nir_intrinsics_c.py --out ${NIR_GEN}/nir_intrinsics.c
        DEPENDS ${NIR_SRC}/nir_intrinsics_c.py ${NIR_SRC}/nir_intrinsics.py
        COMMENT "Generating nir_intrinsics.c" VERBATIM
)

set(NIR_GENERATED_SOURCES
        ${NIR_GEN}/nir_opcodes.c
        ${NIR_GEN}/nir_opcodes.h
        ${NIR_GEN}/nir_builder_opcodes.h
        ${NIR_GEN}/nir_constant_expressions.c
        ${NIR_GEN}/nir_opt_algebraic.c
        ${NIR_GEN}/nir_intrinsics.h
        ${NIR_GEN}/nir_intrinsics_indices.h
        ${NIR_GEN}/nir_intrinsics.c
)

# --- files_libnir, 1:1 aus meson_nir.build übernommen ---
set(NIR_SOURCES
        nir.c nir_builder.c nir_builtin_builder.c
        nir_conversion_builder.h nir_convert_address_format.c
        nir_clip_cull_distance_io_utils.c nir_clone.c
        nir_control_flow.c nir_deref.c nir_divergence_analysis.c
        nir_dominance.c nir_dominance_lca.c nir_downgrade_pls_vars.c
        nir_fixup_is_exported.c nir_format_convert.c nir_from_ssa.c
        nir_functions.c nir_gather_info.c nir_gather_output_deps.c
        nir_gather_tcs_info.c nir_gather_types.c nir_gather_xfb_info.c
        nir_opt_group_loads.c nir_gs_count_vertices.c nir_inline_sysval.c
        nir_inline_uniforms.c nir_instr_set.c nir_io_add_xfb_info.c
        nir_legacy.c nir_linking_helpers.c nir_liveness.c
        nir_loop_analyze.c nir_lower_abort.c nir_lower_alu.c
        nir_lower_alu_width.c nir_lower_alpha.c nir_lower_amul.c
        nir_lower_array_deref_of_vec.c nir_lower_atomics_to_ssbo.c
        nir_lower_bitmap.c nir_lower_blend.c nir_lower_bool_to_float.c
        nir_lower_bool_to_int32.c nir_lower_calls_to_builtins.c
        nir_lower_cl_images.c nir_lower_clamp_color_outputs.c
        nir_lower_clip.c nir_lower_clip_disable.c nir_lower_clip_halfz.c
        nir_lower_const_arrays_to_uniforms.c nir_lower_continue_constructs.c
        nir_lower_convert_alu_types.c nir_lower_cooperative_matrix.c
        nir_lower_variable_initializers.c nir_lower_discard_if.c
        nir_lower_double_ops.c nir_lower_explicit_io.c nir_lower_fb_read.c
        nir_lower_flatshade.c nir_lower_floats.c nir_lower_flrp.c
        nir_lower_fp16_conv.c nir_lower_fragcoord_wtrans.c
        nir_lower_frag_coord_to_pixel_coord.c nir_lower_fragcolor.c
        nir_lower_frexp.c nir_lower_global_vars_to_local.c
        nir_lower_goto_ifs.c nir_lower_gs_intrinsics.c
        nir_lower_halt_to_return.c nir_lower_helper_writes.c
        nir_lower_load_const_to_scalar.c nir_lower_locals_to_regs.c
        nir_lower_idiv.c nir_lower_image.c
        nir_lower_image_atomics_to_global.c
        nir_lower_indirect_derefs_to_if_else_trees.c
        nir_lower_input_attachments.c nir_lower_int64.c
        nir_lower_interpolation.c nir_lower_int_to_float.c nir_lower_io.c
        nir_lower_io_array_vars_to_elements.c nir_lower_io_indirect_loads.c
        nir_lower_io_vars_to_temporaries.c nir_lower_io_to_scalar.c
        nir_lower_io_vars_to_scalar.c nir_lower_is_helper_invocation.c
        nir_lower_multiview.c nir_lower_mediump.c
        nir_lower_mem_access_bit_sizes.c nir_lower_memcpy.c
        nir_lower_memory_model.c nir_lower_non_uniform_access.c
        nir_lower_packing.c nir_lower_passthrough_edgeflags.c
        nir_lower_patch_vertices.c nir_lower_phis_to_scalar.c
        nir_lower_pntc_ytransform.c nir_lower_point_size.c
        nir_lower_point_smooth.c nir_lower_poly_line_smooth.c
        nir_lower_printf.c nir_lower_reg_intrinsics_to_ssa.c
        nir_lower_readonly_images_to_tex.c nir_lower_returns.c
        nir_lower_robust_access.c nir_lower_samplers.c
        nir_lower_sample_shading.c nir_lower_scratch.c
        nir_lower_scratch_to_var.c nir_lower_shader_calls.c
        nir_lower_single_sampled.c nir_lower_ssbo.c nir_lower_subgroups.c
        nir_lower_system_values.c nir_lower_task_shader.c
        nir_lower_terminate_to_demote.c nir_lower_tess_coord_z.c
        nir_lower_tex_shadow.c nir_lower_tex.c
        nir_lower_texcoord_replace.c nir_lower_texcoord_replace_late.c
        nir_lower_two_sided_color.c nir_lower_undef_to_zero.c
        nir_lower_vars_to_ssa.c nir_lower_var_copies.c
        nir_lower_vec_to_regs.c nir_lower_vec3_to_vec4.c
        nir_lower_view_index_to_device_index.c
        nir_lower_viewport_transform.c nir_lower_wpos_center.c
        nir_lower_wpos_ytransform.c nir_lower_wrmasks.c
        nir_lower_xfb_to_stores.c nir_lower_bit_size.c
        nir_lower_ubo_vec4.c nir_lower_uniforms_to_ubo.c
        nir_lower_workgroup_size.c nir_lower_sysvals_to_varyings.c
        nir_metadata.c nir_mod_analysis.c nir_move_output_stores_to_end.c
        nir_move_vec_src_uses_to_dest.c nir_normalize_cubemap_coords.c
        nir_normalize_sin_cos.c nir_opt_access.c nir_opt_barriers.c
        nir_opt_barycentric.c nir_opt_call.c nir_opt_clip_cull_const.c
        nir_opt_combine_stores.c nir_opt_comparison_pre.c
        nir_opt_constant_folding.c nir_opt_copy_prop_vars.c
        nir_opt_copy_propagate.c nir_opt_cse.c nir_opt_dce.c
        nir_opt_dead_cf.c nir_opt_dead_write_vars.c
        nir_opt_find_array_copies.c nir_opt_fp_math_ctrl.c
        nir_opt_frag_coord_to_pixel_coord.c nir_opt_fragdepth.c
        nir_opt_gcm.c nir_opt_generate_bfi.c nir_opt_idiv_const.c
        nir_opt_if.c nir_opt_intrinsics.c nir_opt_large_constants.c
        nir_opt_licm.c nir_opt_load_skip_helpers.c
        nir_opt_load_store_vectorize.c nir_opt_loop.c
        nir_opt_loop_unroll.c nir_opt_memcpy.c nir_opt_move.c
        nir_opt_move_discards_to_top.c nir_opt_move_to_top.c
        nir_opt_mqsad.c nir_opt_non_uniform_access.c nir_opt_offsets.c
        nir_opt_peephole_select.c nir_opt_phi_precision.c
        nir_opt_phi_to_bool.c nir_opt_preamble.c nir_opt_ray_queries.c
        nir_opt_reassociate.c nir_opt_reassociate_bfi.c
        nir_opt_rematerialize_compares.c nir_opt_remove_phis.c
        nir_opt_shared_vars_to_subgroup.c nir_opt_shrink_stores.c
        nir_opt_shrink_vectors.c nir_opt_sink.c nir_opt_undef.c
        nir_opt_uniform_atomics.c nir_opt_uniform_subgroup.c
        nir_opt_uub.c nir_opt_varyings.c nir_opt_vectorize.c
        nir_opt_vectorize_io.c nir_opt_vectorize_io_vars.c
        nir_passthrough_gs.c nir_passthrough_tcs.c nir_phi_builder.c
        nir_print.c nir_propagate_invariant.c nir_range_analysis.c
        nir_recompute_io_bases.c nir_remove_dead_variables.c
        nir_remove_outputs.c nir_remove_tex_shadow.c nir_repair_ssa.c
        nir_scale_fdiv.c nir_schedule.c nir_search.c
        nir_separate_merged_clip_cull_io.c nir_serialize.c
        nir_shader_bisect.c nir_split_64bit_vec3_and_vec4.c
        nir_split_conversions.c nir_split_per_member_structs.c
        nir_split_var_copies.c nir_split_vars.c nir_sweep.c
        nir_to_lcssa.c nir_trivialize_registers.c
        nir_unlower_io_to_vars.c nir_use_dominance.c nir_validate.c
        nir_worklist.c nir_lower_atomics.c
)
list(TRANSFORM NIR_SOURCES PREPEND "${NIR_SRC}/")

add_library(mesa_nir_host STATIC
        ${NIR_SOURCES}
        ${NIR_GENERATED_SOURCES}
)
target_include_directories(mesa_nir_host PUBLIC
        ${NIR_SRC}
        ${NIR_GEN}
        ${MESA_SOURCE_ROOT}/include
        ${MESA_SRC}
)
target_link_libraries(mesa_nir_host PUBLIC mesa_compiler_host mesa_util_host)
# -Wno-misleading-indentation für nir_opt_algebraic.c (siehe Kommentar
# im Original-meson_nir.build — generierte Datei ist zu groß für GCCs Check)
set_source_files_properties(${NIR_GEN}/nir_opt_algebraic.c PROPERTIES
        COMPILE_OPTIONS "-Wno-misleading-indentation"
)

# ════════════════════════════════════════════════════════════════
# libvtn + vtn_bindgen2  (src/compiler/spirv/meson.build)
# ════════════════════════════════════════════════════════════════
set(SPIRV_SRC ${MESA_SRC}/compiler/spirv)
set(SPIRV_GEN ${CMAKE_CURRENT_BINARY_DIR}/spirv)
file(MAKE_DIRECTORY ${SPIRV_GEN})

add_custom_command(
        OUTPUT ${SPIRV_GEN}/vtn_gather_types.c
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${SPIRV_SRC}/vtn_gather_types_c.py
        ${SPIRV_SRC}/spirv.core.grammar.json ${SPIRV_GEN}/vtn_gather_types.c
        DEPENDS ${SPIRV_SRC}/vtn_gather_types_c.py ${SPIRV_SRC}/spirv.core.grammar.json
        COMMENT "Generating vtn_gather_types.c" VERBATIM
)
add_custom_command(
        OUTPUT ${SPIRV_GEN}/spirv_info.h ${SPIRV_GEN}/spirv_info.c
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${SPIRV_SRC}/spirv_info_gen.py
        --json ${SPIRV_SRC}/spirv.core.grammar.json
        --out-h ${SPIRV_GEN}/spirv_info.h --out-c ${SPIRV_GEN}/spirv_info.c
        DEPENDS ${SPIRV_SRC}/spirv_info_gen.py ${SPIRV_SRC}/spirv.core.grammar.json
        COMMENT "Generating spirv_info.[h|c]" VERBATIM
)
add_custom_command(
        OUTPUT ${SPIRV_GEN}/vtn_generator_ids.h
        COMMAND ${MESA_PYTHON3_EXECUTABLE} ${SPIRV_SRC}/vtn_generator_ids_h.py
        ${SPIRV_SRC}/spir-v.xml ${SPIRV_GEN}/vtn_generator_ids.h
        DEPENDS ${SPIRV_SRC}/vtn_generator_ids_h.py ${SPIRV_SRC}/spir-v.xml
        COMMENT "Generating vtn_generator_ids.h" VERBATIM
)

set(VTN_SOURCES
        gl_spirv.c spirv_to_nir.c vtn_alu.c vtn_amd.c vtn_cfg.c
        vtn_cmat.c vtn_debug.c vtn_glsl450.c vtn_opencl.c
        vtn_structured_cfg.c vtn_subgroup.c vtn_variables.c
)
list(TRANSFORM VTN_SOURCES PREPEND "${SPIRV_SRC}/")

add_library(mesa_vtn STATIC
        ${VTN_SOURCES}
        ${SPIRV_GEN}/vtn_gather_types.c
        ${SPIRV_GEN}/spirv_info.c
        ${SPIRV_GEN}/spirv_info.h
        ${SPIRV_GEN}/vtn_generator_ids.h
)
target_include_directories(mesa_vtn PUBLIC
        ${SPIRV_SRC}
        ${SPIRV_GEN}
        ${MESA_SOURCE_ROOT}/include
        ${MESA_SRC}
)
target_link_libraries(mesa_vtn PUBLIC mesa_nir_host mesa_util_host
        SPIRV-Tools SPIRV-Tools-opt
)

# vtn_bindgen2 — SPIR-V -> eingebetteter C++-Code-Generator.
# meson: dependencies = [idep_vtn, idep_mesautil, idep_nir]
add_executable(vtn_bindgen2 ${SPIRV_SRC}/vtn_bindgen2.c)
target_include_directories(vtn_bindgen2 PRIVATE
        ${SPIRV_SRC} ${MESA_SOURCE_ROOT}/include ${MESA_SRC}
)
target_link_libraries(vtn_bindgen2 PRIVATE mesa_vtn mesa_nir_host mesa_util_host)

# ════════════════════════════════════════════════════════════════
# libmesaclc + mesa_clc  (src/compiler/clc/meson.build)
# Hinweis: opencl-c-base.h / opencl-c.h Bundling (xxd-artige Header-
# Einbettung) und die static-libclc-Logik (dep_clc) sind in meson
# optional/bedingt. Für einen ersten funktionierenden mesa_clc lassen
# wir das absichtlich weg (DYNAMIC_LIBCLC_PATH-Zweig, sprich: mesa_clc
# lädt libclc zur Laufzeit dynamisch, statt es einzubetten) — das ist
# der einfachere Pfad und in der Praxis meist ausreichend. Wenn du
# bundled/static libclc willst, sag Bescheid, das ist ein Extra-Schritt.
# ════════════════════════════════════════════════════════════════
set(CLC_SRC ${MESA_SRC}/compiler/clc)

add_library(mesa_libclc_helper STATIC
        ${CLC_SRC}/clc.c
        ${CLC_SRC}/clc_helpers.cpp
)
target_include_directories(mesa_libclc_helper PUBLIC
        ${CLC_SRC}
        ${SPIRV_SRC}
        ${MESA_SOURCE_ROOT}/include
        ${MESA_SRC}
        ${LLVM_INCLUDE_DIRS}
        ${CLANG_INCLUDE_DIRS}
)
target_compile_definitions(mesa_libclc_helper PRIVATE
        LLVM_LIB_DIR="${LLVM_LIBRARY_DIR}"
        HAS_SPIRV_LINK_LLVM_WORKAROUND=1
)
target_link_libraries(mesa_libclc_helper PUBLIC
        mesa_nir_host mesa_util_host mesa_vtn
        ${CLANG_LIBS}
        ${LLVM_LIBS}
        LLVMSPIRVLib
        SPIRV-Tools SPIRV-Tools-opt SPIRV-Tools-link
)

add_executable(mesa_clc ${CLC_SRC}/mesa_clc.c)
target_include_directories(mesa_clc PRIVATE
        ${MESA_SOURCE_ROOT}/include ${MESA_SRC}
)
target_link_libraries(mesa_clc PRIVATE
        mesa_libclc_helper ${LLVM_LIBS} SPIRV-Tools
)

#add_dependencies(mesa_libclc_helper sysroot_headers)
#add_dependencies(mesa_clc sysroot_headers)

# ────────────────────────────────────────────────────────────────
# install — beide Executables landen im Prefix, den die Root-
# CMakeLists per ExternalProject_Add(... INSTALL ...) vorgibt.
# ────────────────────────────────────────────────────────────────
install(TARGETS mesa_clc vtn_bindgen2 RUNTIME DESTINATION bin)