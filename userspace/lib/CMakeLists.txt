cmake_minimum_required(VERSION 3.16)
project(vesplib C ASM_NASM)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

enable_language(ASM_NASM)

set(CPU_TYPE "x86_64")
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

if(NOT DEFINED CMAKE_ASM_NASM_COMPILER AND DEFINED ENV{ASM_NASM})
    set(CMAKE_ASM_NASM_COMPILER $ENV{ASM_NASM})
endif()
if(NOT CMAKE_ASM_NASM_COMPILER)
    find_program(CMAKE_ASM_NASM_COMPILER nasm)
endif()
message(STATUS "CMAKE_ASM_NASM_COMPILER = ${CMAKE_ASM_NASM_COMPILER}")

set(LIBJPEG_SRC "${CMAKE_CURRENT_SOURCE_DIR}/libjpeg/src")
set(NASM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libjpeg/simd/nasm")
set(SIMD_X86_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libjpeg/simd/x86_64")
set(VESPERA_UAPI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/uapi/")

string(TIMESTAMP BUILD_DATE "%Y%m%d")
add_compile_definitions(BUILD_DEF="${BUILD_DATE}")

# ────────────────────────────────────────────────────────────────
# Sysroot resolution (authoritative: root CMakeLists.txt forwards
# VESPERA_SYSROOT here via add_subdirectory scope)
# ────────────────────────────────────────────────────────────────
if(NOT DEFINED VESPERA_SYSROOT)
    if(DEFINED ENV{VESPERA_SYSROOT})
        set(VESPERA_SYSROOT "$ENV{VESPERA_SYSROOT}")
    else()
        set(VESPERA_SYSROOT "${CMAKE_SOURCE_DIR}/../VesperaSysroot")
    endif()
endif()
get_filename_component(VESPERA_SYSROOT "${VESPERA_SYSROOT}" ABSOLUTE)
message(STATUS "vesplib – sysroot: ${VESPERA_SYSROOT}")

if(VESPERA_COMPILER STREQUAL "clang")
    execute_process(
            COMMAND ${CMAKE_C_COMPILER} -print-resource-dir
            OUTPUT_VARIABLE COMPILER_RESOURCE_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(COMPILER_BUILTIN_INCLUDE "${COMPILER_RESOURCE_DIR}/include")
elseif(VESPERA_COMPILER STREQUAL "gcc")
    execute_process(
            COMMAND ${CMAKE_C_COMPILER} -print-file-name=include
            OUTPUT_VARIABLE COMPILER_BUILTIN_INCLUDE
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

message(STATUS "Compiler Builtin Include Dir: ${COMPILER_BUILTIN_INCLUDE}")

file(MAKE_DIRECTORY
        ${VESPERA_SYSROOT}/usr/include
        ${VESPERA_SYSROOT}/usr/include/vespera/dev
        ${VESPERA_SYSROOT}/usr/include/sys
        ${VESPERA_SYSROOT}/usr/lib
        ${VESPERA_SYSROOT}/bin
        ${VESPERA_SYSROOT}/lib
        ${VESPERA_SYSROOT}/etc/fonts
        ${VESPERA_SYSROOT}/tmp
        ${VESPERA_SYSROOT}/mnt
        ${VESPERA_SYSROOT}/var/log
        ${VESPERA_SYSROOT}/home/vespera
        ${VESPERA_SYSROOT}/root
)

install(DIRECTORY
        ${VESPERA_UAPI_INCLUDE_DIR}
        DESTINATION ${VESPERA_SYSROOT}/usr/include/
        COMPONENT headers
)

install(DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${VESPERA_SYSROOT}/usr/include
        COMPONENT headers
)

install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/lua/include/lua.h
        ${CMAKE_CURRENT_SOURCE_DIR}/lua/include/lauxlib.h
        ${CMAKE_CURRENT_SOURCE_DIR}/lua/include/lualib.h
        ${CMAKE_CURRENT_SOURCE_DIR}/lua/include/luaconf.h
        DESTINATION ${VESPERA_SYSROOT}/usr/include/lua
        COMPONENT headers
)

add_custom_target(sysroot_headers
        COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component headers
        COMMENT "Installing headers into sysroot"
)

add_subdirectory(lua lua_build)
add_subdirectory(readline)
add_subdirectory(lvespera)
add_subdirectory(ld)
add_subdirectory(mesa mesa_build)

# ────────────────────────────────────────────────────────────────
# Math library (generic + x86_64 SIMD overrides)
# ────────────────────────────────────────────────────────────────
set(MATH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/math)

file(GLOB MATH_GENERIC_C ${MATH_DIR}/*.c)
file(GLOB MATH_X86_64_C  ${MATH_DIR}/x86_64/*.c)
file(GLOB MATH_X86_64_S  ${MATH_DIR}/x86_64/*.s)

foreach(_override ${MATH_X86_64_C} ${MATH_X86_64_S})
    get_filename_component(_name ${_override} NAME_WE)
    foreach(_generic ${MATH_GENERIC_C})
        get_filename_component(_gname ${_generic} NAME_WE)
        if(_gname STREQUAL _name)
            list(REMOVE_ITEM MATH_GENERIC_C ${_generic})
        endif()
    endforeach()
endforeach()

set(MATH_SOURCES ${MATH_GENERIC_C} ${MATH_X86_64_C} ${MATH_X86_64_S})

foreach(_src ${MATH_SOURCES})
    get_filename_component(_ext ${_src} EXT)
    if(_ext STREQUAL ".s")
        set_source_files_properties(${_src} PROPERTIES COMPILE_FLAGS "")
    else()
        set_source_files_properties(${_src} PROPERTIES
                COMPILE_FLAGS
                "-nostdinc -fno-stack-protector -O2 \
                 -I${MATH_DIR} \
                 -include ${MATH_DIR}/features.h \
                 --sysroot=${VESPERA_SYSROOT} \
                 -isystem ${VESPERA_SYSROOT}/usr/include \
                 -idirafter ${COMPILER_BUILTIN_INCLUDE}"
        )
    endif()
endforeach()

# ────────────────────────────────────────────────────────────────
# vesplib (C runtime + libjpeg + math)
# ────────────────────────────────────────────────────────────────
set(LIB_SOURCES
        src/errno.c
        src/channel.c
        src/exec.c
        src/malloc.c
        src/realm.c
        src/reboot.c
        src/stdio.c
        src/stdlib.c
        src/string.c
        src/sysstd.c
        src/urandom.c
        src/ctype.c
        src/mount.c
        src/time.c
        src/signal.c
        src/locale.c
        src/vbus.c
        src/wchar.c
        src/unit.c
        src/futex.c
        src/dlfcn.c
        src/unistd.c
        src/fcntl.c
        src/pthread.c
        src/sched.c
        src/strings.c

        src/bits/fenv.c

        src/sys/shm.c
        src/sys/ioctl.c
        src/sys/mman.c
        src/sys/time.c
        src/sys/stat.c
        src/sys/syslog.c
        src/sys/sysinfo.c
        src/sys/file.c

        src/internal/fd_table.c

        ${LIBJPEG_SRC}/jcapimin.c
        ${LIBJPEG_SRC}/jdapimin.c
        ${LIBJPEG_SRC}/wrapper/jdcolor-8.c
        ${LIBJPEG_SRC}/wrapper/jdcoefct-8.c
        ${LIBJPEG_SRC}/wrapper/jddctmgr-8.c
        ${LIBJPEG_SRC}/wrapper/jddiffct-8.c
        ${LIBJPEG_SRC}/jdhuff.c
        ${LIBJPEG_SRC}/jdinput.c
        ${LIBJPEG_SRC}/wrapper/jdmainct-8.c
        ${LIBJPEG_SRC}/jdmarker.c
        ${LIBJPEG_SRC}/jdmaster.c
        ${LIBJPEG_SRC}/wrapper/jdmerge-8.c
        ${LIBJPEG_SRC}/jdphuff.c
        ${LIBJPEG_SRC}/wrapper/jdpostct-8.c
        ${LIBJPEG_SRC}/wrapper/jdsample-8.c
        ${LIBJPEG_SRC}/jdtrans.c
        ${LIBJPEG_SRC}/jerror.c
        ${LIBJPEG_SRC}/jmemmgr.c
        ${LIBJPEG_SRC}/jmemnobs.c
        ${LIBJPEG_SRC}/wrapper/jutils-8.c
        ${LIBJPEG_SRC}/jcparam.c
        ${LIBJPEG_SRC}/jdatadst.c
        ${LIBJPEG_SRC}/jdatasrc.c
        ${LIBJPEG_SRC}/jcomapi.c
        ${LIBJPEG_SRC}/jcmarker.c
        ${LIBJPEG_SRC}/jcinit.c
        ${LIBJPEG_SRC}/jcphuff.c
        ${LIBJPEG_SRC}/jchuff.c
        ${LIBJPEG_SRC}/jcmaster.c
        ${LIBJPEG_SRC}/jfdctflt.c
        ${LIBJPEG_SRC}/wrapper/jdapistd-8.c
        ${LIBJPEG_SRC}/wrapper/jidctint-8.c
        ${LIBJPEG_SRC}/wrapper/jidctflt-8.c
        ${LIBJPEG_SRC}/wrapper/jidctfst-8.c
        ${LIBJPEG_SRC}/wrapper/jidctred-8.c
        ${LIBJPEG_SRC}/wrapper/jquant1-8.c
        ${LIBJPEG_SRC}/wrapper/jquant2-8.c
        ${LIBJPEG_SRC}/wrapper/jdlossls-8.c
        ${LIBJPEG_SRC}/wrapper/jcapistd-8.c
        ${LIBJPEG_SRC}/wrapper/jccolor-8.c
        ${LIBJPEG_SRC}/wrapper/jcsample-8.c
        ${LIBJPEG_SRC}/wrapper/jcprepct-8.c
        ${LIBJPEG_SRC}/wrapper/jcmainct-8.c
        ${LIBJPEG_SRC}/wrapper/jcdctmgr-8.c
        ${LIBJPEG_SRC}/wrapper/jccoefct-8.c
        ${LIBJPEG_SRC}/wrapper/jfdctint-8.c
        ${LIBJPEG_SRC}/wrapper/jfdctfst-8.c

        jpeg/jpeg.c
)

set(SIMD_ASM_SOURCES
        ${SIMD_X86_DIR}/jsimdcpu.asm
        ${SIMD_X86_DIR}/jfdctflt-sse.asm
        ${SIMD_X86_DIR}/jccolor-sse2.asm
        ${SIMD_X86_DIR}/jcgray-sse2.asm
        ${SIMD_X86_DIR}/jchuff-sse2.asm
        ${SIMD_X86_DIR}/jcphuff-sse2.asm
        ${SIMD_X86_DIR}/jcsample-sse2.asm
        ${SIMD_X86_DIR}/jdcolor-sse2.asm
        ${SIMD_X86_DIR}/jdmerge-sse2.asm
        ${SIMD_X86_DIR}/jdsample-sse2.asm
        ${SIMD_X86_DIR}/jfdctfst-sse2.asm
        ${SIMD_X86_DIR}/jfdctint-sse2.asm
        ${SIMD_X86_DIR}/jidctflt-sse2.asm
        ${SIMD_X86_DIR}/jidctfst-sse2.asm
        ${SIMD_X86_DIR}/jidctint-sse2.asm
        ${SIMD_X86_DIR}/jidctred-sse2.asm
        ${SIMD_X86_DIR}/jquantf-sse2.asm
        ${SIMD_X86_DIR}/jquanti-sse2.asm
        ${SIMD_X86_DIR}/jccolor-avx2.asm
        ${SIMD_X86_DIR}/jcgray-avx2.asm
        ${SIMD_X86_DIR}/jcsample-avx2.asm
        ${SIMD_X86_DIR}/jdcolor-avx2.asm
        ${SIMD_X86_DIR}/jdmerge-avx2.asm
        ${SIMD_X86_DIR}/jdsample-avx2.asm
        ${SIMD_X86_DIR}/jfdctint-avx2.asm
        ${SIMD_X86_DIR}/jidctint-avx2.asm
        ${SIMD_X86_DIR}/jquanti-avx2.asm
)

foreach(_asm IN LISTS SIMD_ASM_SOURCES)
    set_source_files_properties(${_asm}
            PROPERTIES
            LANGUAGE     ASM_NASM
            COMPILE_FLAGS
            "-f elf64 -I${NASM_INCLUDE_DIR} -I${SIMD_X86_DIR} -D__x86_64__"
            OBJECT_DEPENDS
            "${NASM_INCLUDE_DIR}/jsimdcfg.inc;${NASM_INCLUDE_DIR}/jsimdext.inc"
    )
endforeach()

set(ASM_SOURCES src/setjmp.asm)

add_library(vesplib STATIC
        ${SIMD_ASM_SOURCES}
        ${LIB_SOURCES}
        ${ASM_SOURCES}
        ${MATH_SOURCES}
)

set_source_files_properties(
        src/crt0.asm
        src/setjmp.asm
        PROPERTIES
        LANGUAGE      ASM_NASM
        COMPILE_FLAGS "-f elf64"
)

set_target_properties(vesplib PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME "vesplib"
)

target_include_directories(vesplib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${VESPERA_UAPI_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/libjpeg/include
        ${CMAKE_CURRENT_SOURCE_DIR}/${LIBJPEG_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/libjpeg/simd
        ${NASM_INCLUDE_DIR}
        ${SIMD_X86_DIR}
)

target_compile_options(vesplib PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>
        $<$<COMPILE_LANGUAGE:C>:-fno-builtin>
        $<$<COMPILE_LANGUAGE:C>:-nostdinc>
        $<$<COMPILE_LANGUAGE:C>:-Wall>
        $<$<COMPILE_LANGUAGE:C>:-Wextra>
        $<$<COMPILE_LANGUAGE:C>:-O2>
        $<$<COMPILE_LANGUAGE:C>:-fPIC>
)

# ────────────────────────────────────────────────────────────────
# crt0 (startup stub linked into every userspace binary)
# ────────────────────────────────────────────────────────────────
add_library(crt0_obj OBJECT
        src/crt0.asm
        src/entry.c
)

set_source_files_properties(src/crt0.asm
        PROPERTIES
        LANGUAGE      ASM_NASM
        COMPILE_FLAGS "-f elf64"
)

target_include_directories(crt0_obj PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${VESPERA_UAPI_INCLUDE_DIR}
)

target_compile_options(crt0_obj PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>
        $<$<COMPILE_LANGUAGE:C>:-fno-builtin>
        $<$<COMPILE_LANGUAGE:C>:-nostdinc>
        $<$<COMPILE_LANGUAGE:C>:-O0>
        $<$<COMPILE_LANGUAGE:C>:-fPIC>
)

# ────────────────────────────────────────────────────────────────
# vesp++ (C++ runtime support)
# ────────────────────────────────────────────────────────────────
add_library(vesplib_cpp STATIC src/c++/cxx_runtime.cpp)

target_compile_options(vesplib_cpp PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-stack-protector>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-builtin>
        $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>
        $<$<COMPILE_LANGUAGE:CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-O2>
        $<$<COMPILE_LANGUAGE:CXX>:-fPIC>
)

target_include_directories(vesplib_cpp PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/c++
)

target_link_libraries(vesplib_cpp PUBLIC vesplib)

set_target_properties(vesplib_cpp PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME "vesp++"
)

# ────────────────────────────────────────────────────────────────
#   interpreter binary (system-level; higher-level apps in VesperaApps)
# ────────────────────────────────────────────────────────────────
set(COMMON_C_FLAGS
        -target x86_64-unknown-none-elf
        -nostdlib -nostdinc
        -fno-stack-protector -fPIE -g -O2 -Wall
        --sysroot=${VESPERA_SYSROOT}
        "-isystem${VESPERA_SYSROOT}/usr/include"
        "-idirafter ${COMPILER_BUILTIN_INCLUDE}"
)

add_executable(lua_bin
        $<TARGET_OBJECTS:crt0_obj>
        ${CMAKE_CURRENT_SOURCE_DIR}/../bin/lua.c
)

target_compile_options(lua_bin PRIVATE $<$<COMPILE_LANGUAGE:C>:${COMMON_C_FLAGS}>)

target_include_directories(lua_bin PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lua/include
        "${VESPERA_SYSROOT}/usr/include"
)

target_link_libraries(lua_bin PRIVATE lua lvespera readline vesplib)

set(USER_LD_FLAGS
        "-nostdlib -target x86_64-unknown-linux-gnu -fno-stack-protector -pie -Wl,--no-dynamic-linker \
     --sysroot=${VESPERA_SYSROOT} -L${VESPERA_SYSROOT}/usr/lib"
)

set_target_properties(lua_bin PROPERTIES
        OUTPUT_NAME "lua"
        LINK_FLAGS  "${USER_LD_FLAGS}"
)

install(TARGETS lua_bin RUNTIME DESTINATION "${VESPERA_SYSROOT}/bin" COMPONENT runtime)

# Libraries — copy-if-different semantics via cmake --install
install(FILES       $<TARGET_FILE:vesplib>
        DESTINATION "${VESPERA_SYSROOT}/usr/lib"
        RENAME      "libvesp.a"
        COMPONENT   runtime
)
install(FILES       $<TARGET_FILE:vesplib_cpp>
        DESTINATION "${VESPERA_SYSROOT}/usr/lib"
        COMPONENT   runtime
)

# Static sysroot data directories
install(DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}/../etc/
        DESTINATION "${VESPERA_SYSROOT}/etc"
        COMPONENT   runtime
)
install(DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}/../usr/
        DESTINATION "${VESPERA_SYSROOT}/usr"
        COMPONENT   runtime
)

# ────────────────────────────────────────────────────────────────
# LVGL sysroot install
# ────────────────────────────────────────────────────────────────
add_custom_target(sysroot_install_lvgl ALL
        DEPENDS lvgl_project
        COMMAND ${CMAKE_COMMAND} -E make_directory ${VESPERA_SYSROOT}/usr/lib
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${LVGL_BINARY_DIR}/lib/liblvgl.a
        ${VESPERA_SYSROOT}/usr/lib/liblvgl.a
        COMMENT "Installing LVGL into sysroot"
)

add_dependencies(mesa_root sysroot_headers)
add_dependencies(lua sysroot_headers)
add_dependencies(lua_bin sysroot_headers)
add_dependencies(libreadline sysroot_headers)
add_dependencies(lvespera sysroot_headers)
add_dependencies(vesplib sysroot_headers)

# sysroot_install — build all libs and populate the sysroot.
add_custom_target(sysroot_install
        DEPENDS
        vesplib vesplib_cpp crt0_obj
        lua lua_bin lvespera ld_vespera mesa_root
        sysroot_install_readline

        # Link crt0 into a staging file first, then copy_if_different so downstream
        # targets are not invalidated when the content has not actually changed.
        COMMAND ld -r
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/crt0_obj.dir/src/crt0.asm.o
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/crt0_obj.dir/src/entry.c.o
        -o ${CMAKE_CURRENT_BINARY_DIR}/crt0_linked.o
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/crt0_linked.o
        ${VESPERA_SYSROOT}/usr/lib/crt0.o


        COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component runtime

        COMMENT "Populating sysroot with libraries and headers"
)