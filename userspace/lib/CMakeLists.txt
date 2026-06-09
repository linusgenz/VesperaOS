cmake_minimum_required(VERSION 3.16)
project(vesplib C ASM_NASM)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

enable_language(ASM_NASM)

set(CPU_TYPE "x86_64")
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

if (NOT DEFINED CMAKE_ASM_NASM_COMPILER AND DEFINED ENV{ASM_NASM})
    set(CMAKE_ASM_NASM_COMPILER $ENV{ASM_NASM})
endif ()
if (NOT CMAKE_ASM_NASM_COMPILER)
    find_program(CMAKE_ASM_NASM_COMPILER nasm)
endif ()
message(STATUS "CMAKE_ASM_NASM_COMPILER = ${CMAKE_ASM_NASM_COMPILER}")

set(LIBJPEG_SRC "${CMAKE_CURRENT_SOURCE_DIR}/libjpeg/src")
set(NASM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libjpeg/simd/nasm")
set(SIMD_X86_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libjpeg/simd/x86_64")

string(TIMESTAMP BUILD_DATE "%Y%m%d")
add_compile_definitions(BUILD_DEF="${BUILD_DATE}")

if (NOT DEFINED VESPERA_SYSROOT)
    if (DEFINED ENV{VESPERA_SYSROOT})
        set(VESPERA_SYSROOT "$ENV{VESPERA_SYSROOT}")
    else ()
        set(VESPERA_SYSROOT "${CMAKE_SOURCE_DIR}/../VesperaSysroot")
    endif ()
endif ()

get_filename_component(VESPERA_SYSROOT "${VESPERA_SYSROOT}" ABSOLUTE)
message(STATUS "Using VESPERA_SYSROOT: ${VESPERA_SYSROOT}")

add_custom_target(prepare_sysroot

        COMMAND ${CMAKE_COMMAND} -E make_directory
        ${VESPERA_SYSROOT}/usr/include
        ${VESPERA_SYSROOT}/usr/include/vespera/dev
        ${VESPERA_SYSROOT}/usr/include/sys
        ${VESPERA_SYSROOT}/usr/lib
        ${VESPERA_SYSROOT}/bin
        ${VESPERA_SYSROOT}/lib
        ${VESPERA_SYSROOT}/etc
        ${VESPERA_SYSROOT}/etc/fonts
        ${VESPERA_SYSROOT}/tmp
        ${VESPERA_SYSROOT}/mnt
        ${VESPERA_SYSROOT}/var/log
        ${VESPERA_SYSROOT}/home
        ${VESPERA_SYSROOT}/home/vespera
        ${VESPERA_SYSROOT}/root

        COMMAND ${CMAKE_COMMAND} -E make_directory
        ${VESPERA_SYSROOT}/usr/include
        ${VESPERA_SYSROOT}/usr/include/vespera/dev
        ${VESPERA_SYSROOT}/usr/include/sys

        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/include/uapi/vespera
        ${VESPERA_SYSROOT}/usr/include/vespera

        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/userspace/lib/include
        ${VESPERA_SYSROOT}/usr/include

        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/userspace/lib/lua/include/luaconf.h
        ${VESPERA_SYSROOT}/usr/include/lua/luaconf.h

        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/userspace/lib/lua/include/lauxlib.h
        ${VESPERA_SYSROOT}/usr/include/lua/lauxlib.h

        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/userspace/lib/lua/include/lualib.h
        ${VESPERA_SYSROOT}/usr/include/lua/lualib.h

        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/userspace/lib/lua/include/lualib.h
        ${VESPERA_SYSROOT}/usr/include/lua/lualib.h

        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/userspace/lib/lua/include/lua.h
        ${VESPERA_SYSROOT}/usr/include/lua/lua.h

        COMMENT "Copying headers to sysroot"
)

add_subdirectory(lua lua_build)
add_subdirectory(readline)
add_subdirectory(lvespera)
add_subdirectory(stella)

include(ExternalProject)

# Pfade definieren
set(LVGL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lvgl/")
set(LVGL_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/lvgl_external_build")

ExternalProject_Add(lvgl_project
        SOURCE_DIR ${LVGL_SOURCE_DIR}
        BINARY_DIR ${LVGL_BINARY_DIR}

        CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/lvgl/vespera_toolchain.cmake
        -DLV_BUILD_CONF_DIR=${CMAKE_CURRENT_SOURCE_DIR}/lvgl

        INSTALL_COMMAND ""
)

set(MATH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/math)

file(GLOB MATH_GENERIC_C ${MATH_DIR}/*.c)
file(GLOB MATH_X86_64_C ${MATH_DIR}/x86_64/*.c)
file(GLOB MATH_X86_64_S ${MATH_DIR}/x86_64/*.s)

foreach (_override ${MATH_X86_64_C} ${MATH_X86_64_S})
    get_filename_component(_name ${_override} NAME_WE)  # z.B. "sqrt"
    foreach (_generic ${MATH_GENERIC_C})
        get_filename_component(_gname ${_generic} NAME_WE)
        if (_gname STREQUAL _name)
            list(REMOVE_ITEM MATH_GENERIC_C ${_generic})
        endif ()
    endforeach ()
endforeach ()

set(MATH_SOURCES
        ${MATH_GENERIC_C}
        ${MATH_X86_64_C}
        ${MATH_X86_64_S}
)

foreach (_src ${MATH_SOURCES})
    get_filename_component(_ext ${_src} EXT)
    if (_ext STREQUAL ".s")
        set_source_files_properties(${_src} PROPERTIES
                COMPILE_FLAGS ""
        )
    else ()
        set_source_files_properties(${_src} PROPERTIES
                COMPILE_FLAGS " -nostdinc -fno-stack-protector -O2 \
                           -I${MATH_DIR} \
                           -include ${MATH_DIR}/features.h \
                           --sysroot=${VESPERA_SYSROOT} \
                           -isystem ${VESPERA_SYSROOT}/usr/include"
        )
    endif ()
endforeach ()

# --- C Sources ---
set(LIB_SOURCES
        src/errno.c
        src/channel.c
        src/exec.c
        src/file.c
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
        src/stat.c

        src/bits/fenv.c

        src/sys/shm.c
        src/sys/ioctl.c
        src/sys/mman.c

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

        #${SIMD_X86_DIR}/jsimd.c

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

foreach (_asm IN LISTS SIMD_ASM_SOURCES)
    set_source_files_properties(${_asm}
            PROPERTIES
            LANGUAGE ASM_NASM
            COMPILE_FLAGS "-f elf64 -I${NASM_INCLUDE_DIR} -I${SIMD_X86_DIR} -D__x86_64__"
            OBJECT_DEPENDS "${NASM_INCLUDE_DIR}/jsimdcfg.inc;${NASM_INCLUDE_DIR}/jsimdext.inc"
    )
endforeach ()


# --- ASM Sources ---
set(ASM_SOURCES
        src/setjmp.asm
)

add_library(vesplib STATIC
        ${SIMD_ASM_SOURCES}
        ${LIB_SOURCES}
        ${ASM_SOURCES}
        ${MATH_SOURCES}
)
add_dependencies(vesplib prepare_sysroot)

set_source_files_properties(
        src/crt0.asm
        src/setjmp.asm
        PROPERTIES
        LANGUAGE ASM_NASM
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
        $<$<COMPILE_LANGUAGE:C>:-O0>
        $<$<COMPILE_LANGUAGE:C>:-fPIC>
)

add_library(crt0_obj OBJECT
        src/crt0.asm
        src/entry.c
)
add_dependencies(crt0_obj prepare_sysroot)

set_source_files_properties(src/crt0.asm
        PROPERTIES
        LANGUAGE ASM_NASM
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

set(CXX_LIB_SOURCES
        src/c++/cxx_runtime.cpp
)

# Erstelle die statische C++ Library
add_library(vesplib_cpp STATIC
        ${CXX_LIB_SOURCES}
)
add_dependencies(vesplib_cpp prepare_sysroot)

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

set(COMMON_C_FLAGS
        -target
        x86_64-unknown-linux-gnu
        -nostdlib
        -nostdinc
        -fno-stack-protector
        -fPIE
        -g
        -O0
        -Wall
        --sysroot=${VESPERA_SYSROOT}
        "-isystem${VESPERA_SYSROOT}/usr/include"
)

add_executable(lua_bin
        $<TARGET_OBJECTS:crt0_obj>
        ${CMAKE_SOURCE_DIR}/userspace/bin/lua.c
)

target_compile_options(lua_bin PRIVATE $<$<COMPILE_LANGUAGE:C>:${COMMON_C_FLAGS}>)

target_include_directories(lua_bin PRIVATE
        ${CMAKE_SOURCE_DIR}/userspace/lua/include
        "${VESPERA_SYSROOT}/usr/include"
)

target_link_libraries(lua_bin PRIVATE
        lvespera
        lua
        readline
        vesplib
)

set(USER_LD_FLAGS "-nostdlib -target x86_64-unknown-linux-gnu -fno-stack-protector -pie --sysroot=${VESPERA_SYSROOT} -L${VESPERA_SYSROOT}/usr/lib")

set_target_properties(lua_bin PROPERTIES
        OUTPUT_NAME "lua"
        LINK_FLAGS "${USER_LD_FLAGS}"
)

install(TARGETS lua_bin RUNTIME DESTINATION "${VESPERA_SYSROOT}/bin")

add_dependencies(stella lvgl_project)

add_custom_target(sysroot_install
        DEPENDS vesplib vesplib_cpp crt0_obj lua lua_bin lvespera stella sysroot_install_readline prepare_sysroot sysroot_install_lvgl

        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/userspace/etc/
        ${VESPERA_SYSROOT}/etc/

        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/userspace/usr/
        ${VESPERA_SYSROOT}/usr/

        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:vesplib>
        ${VESPERA_SYSROOT}/usr/lib/libvesp.a

        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/userspace/lib/libvesp++.a
        ${VESPERA_SYSROOT}/usr/lib/libvesp++.a

        COMMAND ld -r
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/crt0_obj.dir/src/crt0.asm.o
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/crt0_obj.dir/src/entry.c.o
        -o ${VESPERA_SYSROOT}/usr/lib/crt0.o

        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_install.cmake

        COMMENT "Populating sysroot and deploying applications"
)

add_custom_target(sysroot_install_lvgl ALL
        DEPENDS lvgl_project

        COMMAND ${CMAKE_COMMAND} -E make_directory ${VESPERA_SYSROOT}/usr/lib
        #COMMAND ${CMAKE_COMMAND} -E make_directory ${VESPERA_SYSROOT}/usr/include/lvgl

        COMMAND ${CMAKE_COMMAND} -E copy ${LVGL_BINARY_DIR}/lib/liblvgl.a ${VESPERA_SYSROOT}/usr/lib/liblvgl.a

        # COMMAND ${CMAKE_COMMAND} -E copy ${LVGL_SOURCE_DIR}/lvgl.h ${VESPERA_SYSROOT}/usr/include/lvgl/lvgl.h

        # COMMAND ${CMAKE_COMMAND} -E copy_directory ${LVGL_SOURCE_DIR}/src ${VESPERA_SYSROOT}/usr/include/lvgl/src

        COMMENT "Automated external build and sysroot install for LVGL"
)