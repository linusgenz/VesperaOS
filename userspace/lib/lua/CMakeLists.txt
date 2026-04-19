file(GLOB LUA_SOURCES src/*.c)

add_library(liblua STATIC ${LUA_SOURCES})

target_include_directories(liblua
        PUBLIC  include
        PRIVATE ${SYSROOT}/usr/include
)

target_compile_options(liblua PRIVATE
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