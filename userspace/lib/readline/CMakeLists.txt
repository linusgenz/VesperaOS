cmake_minimum_required(VERSION 3.16)

set(SYSROOT ${CMAKE_SOURCE_DIR}/sysroot)

add_library(libreadline STATIC
        src/readline.c
)

set_target_properties(libreadline PROPERTIES
        OUTPUT_NAME "readline"
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(libreadline PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/userspace/lib/include
        ${VESPERA_UAPI_INCLUDE_DIR}
)

target_compile_options(libreadline PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
        $<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>
        $<$<COMPILE_LANGUAGE:C>:-fno-builtin>
        $<$<COMPILE_LANGUAGE:C>:-nostdinc>
        $<$<COMPILE_LANGUAGE:C>:-Wall>
        $<$<COMPILE_LANGUAGE:C>:-Wextra>
        $<$<COMPILE_LANGUAGE:C>:-O2>
        $<$<COMPILE_LANGUAGE:C>:-fPIC>
        $<$<COMPILE_LANGUAGE:C>:--sysroot=${SYSROOT}>
        $<$<COMPILE_LANGUAGE:C>:-isystem${SYSROOT}/usr/include>
)

add_custom_target(sysroot_install_readline
        DEPENDS libreadline

        COMMAND ${CMAKE_COMMAND} -E make_directory
        ${SYSROOT}/usr/include/readline

        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/include/readline.h
        ${SYSROOT}/usr/include/readline/readline.h

        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/include/history.h
        ${SYSROOT}/usr/include/readline/history.h

        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:libreadline>
        ${SYSROOT}/usr/lib/libreadline.a

        COMMENT "Installing libreadline into sysroot"
)