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
set(SIMD_X86_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/libjpeg/simd/x86_64")

string(TIMESTAMP BUILD_DATE "%Y%m%d")
add_compile_definitions(BUILD_DEF="${BUILD_DATE}")

add_subdirectory(lua)
add_subdirectory(readline)
add_subdirectory(lvespera)

set(MATH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/math)

file(GLOB MATH_GENERIC_C   ${MATH_DIR}/*.c)
file(GLOB MATH_X86_64_C    ${MATH_DIR}/x86_64/*.c)
file(GLOB MATH_X86_64_S    ${MATH_DIR}/x86_64/*.s)

foreach(_override ${MATH_X86_64_C} ${MATH_X86_64_S})
    get_filename_component(_name ${_override} NAME_WE)  # z.B. "sqrt"
    foreach(_generic ${MATH_GENERIC_C})
        get_filename_component(_gname ${_generic} NAME_WE)
        if(_gname STREQUAL _name)
            list(REMOVE_ITEM MATH_GENERIC_C ${_generic})
        endif()
    endforeach()
endforeach()

set(MATH_SOURCES
        ${MATH_GENERIC_C}
        ${MATH_X86_64_C}
        ${MATH_X86_64_S}
)

foreach(_src ${MATH_SOURCES})
    get_filename_component(_ext ${_src} EXT)
    if(_ext STREQUAL ".s")
        set_source_files_properties(${_src} PROPERTIES
                COMPILE_FLAGS "-ffreestanding"
        )
    else()
        set_source_files_properties(${_src} PROPERTIES
                COMPILE_FLAGS "-ffreestanding -nostdinc -fno-stack-protector -O2 \
                           -I${MATH_DIR} \
                           -include ${MATH_DIR}/features.h \
                           --sysroot=${SYSROOT} \
                           -isystem ${SYSROOT}/usr/include"
        )
    endif()
endforeach()

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

foreach(_asm IN LISTS SIMD_ASM_SOURCES)
    set_source_files_properties(${_asm}
            PROPERTIES
            LANGUAGE ASM_NASM
            COMPILE_FLAGS "-f elf64 -I${NASM_INCLUDE_DIR} -I${SIMD_X86_DIR} -D__x86_64__"
            OBJECT_DEPENDS "${NASM_INCLUDE_DIR}/jsimdcfg.inc;${NASM_INCLUDE_DIR}/jsimdext.inc"
    )
endforeach()


# --- ASM Sources ---
set(ASM_SOURCES
        src/crt0.asm
        src/setjmp.asm
)

add_library(vesplib STATIC
        ${SIMD_ASM_SOURCES}
        ${LIB_SOURCES}
        ${ASM_SOURCES}
        ${MATH_SOURCES}
)

set_source_files_properties(
        src/crt0.asm
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
        $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
        $<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>
        $<$<COMPILE_LANGUAGE:C>:-fno-builtin>
        $<$<COMPILE_LANGUAGE:C>:-nostdinc>
        $<$<COMPILE_LANGUAGE:C>:-Wall>
        $<$<COMPILE_LANGUAGE:C>:-Wextra>
        $<$<COMPILE_LANGUAGE:C>:-O0>
        $<$<COMPILE_LANGUAGE:C>:-fPIC>
)



set(SYSROOT ${CMAKE_SOURCE_DIR}/sysroot)

add_custom_target(sysroot_install
        DEPENDS vesplib sysroot_install_readline

        COMMAND ${CMAKE_COMMAND} -E make_directory
        ${SYSROOT}/usr/include
        ${SYSROOT}/usr/include/vespera/dev
        ${SYSROOT}/usr/include/sys
        ${SYSROOT}/usr/lib
        ${SYSROOT}/bin
        ${SYSROOT}/lib
        ${SYSROOT}/etc
        ${SYSROOT}/etc/fonts
        ${SYSROOT}/tmp
        ${SYSROOT}/mnt
        ${SYSROOT}/var/log

        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/userspace/lib/include
        ${SYSROOT}/usr/include

        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/include/uapi/vespera
        ${SYSROOT}/usr/include/vespera

        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:vesplib>
        ${SYSROOT}/usr/lib/libvesp.a

        COMMENT "Populating sysroot"
)