# lvespera - VesperaOS system module for Lua
add_library(liblvespera STATIC lvespera.c)

target_include_directories(liblvespera PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../lua/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${SYSROOT}/usr/include
    ${CMAKE_SOURCE_DIR}/include/uapi
)

target_compile_options(liblvespera PRIVATE
    -DLUA_USE_VESPERA
    -ffreestanding
    -nostdinc
    -fno-stack-protector
    -fPIE
    -O0
    -g
    --sysroot=${SYSROOT}
    -isystem ${SYSROOT}/usr/include
)