# stella – GUI/window framework for VesperaOS

# add_library(stella STATIC stella.c stella++.cpp)

#target_include_directories(stella PRIVATE
#        ${CMAKE_CURRENT_SOURCE_DIR}/../lvgl/src/
#        ${CMAKE_CURRENT_SOURCE_DIR}/../include
#)

#target_compile_definitions(stella PRIVATE "LV_CONF_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/../lvgl/lv_conf.h\"")

set(STELLA_COMMON_FLAGS
        -ffreestanding
        -nostdlib
        -nostdinc
        -fno-stack-protector
        -fPIE
        -g
        -O0
        -Wall
        --sysroot=${VESPERA_SYSROOT}
)

#target_compile_options(stella PRIVATE $<$<COMPILE_LANGUAGE:C>:
#        ${STELLA_COMMON_FLAGS}
#        "-isystem${VESPERA_SYSROOT}/usr/include"
#        >)

#target_compile_options(stella PRIVATE $<$<COMPILE_LANGUAGE:CXX>:
#        "-isystem${VESPERA_SYSROOT}/usr/include/c++"
#        "-isystem${VESPERA_SYSROOT}/usr/include"
#        ${STELLA_COMMON_FLAGS}
#        -fno-exceptions
#        -fno-rtti
#        -nostdinc++
#        >)

# install(TARGETS stella ARCHIVE DESTINATION ${VESPERA_SYSROOT}/usr/lib COMPONENT runtime)