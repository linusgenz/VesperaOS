# lvespera - VesperaOS system module for Lua
add_library(lvespera STATIC lvespera.c luautil.c)

target_include_directories(lvespera PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../lua/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${VESPERA_SYSROOT}/usr/include
    ${CMAKE_SOURCE_DIR}/include/uapi
)

target_compile_options(lvespera PRIVATE
    -DLUA_USE_VESPERA
    -ffreestanding
    -nostdinc
    -fno-stack-protector
    -fPIE
    -O0
    -g
    --sysroot=${VESPERA_SYSROOT}
    -isystem ${VESPERA_SYSROOT}/usr/include
)

install(TARGETS lvespera ARCHIVE DESTINATION ${VESPERA_SYSROOT}/usr/lib)