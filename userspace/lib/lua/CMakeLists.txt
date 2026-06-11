file(GLOB LUA_SOURCES src/*.c)

add_library(lua STATIC ${LUA_SOURCES})

target_include_directories(lua
        PUBLIC  include
        PRIVATE ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(lua PRIVATE
        -DLUA_USE_VESPERA
        -ffreestanding
        -nostdinc
        -fno-stack-protector
        -fPIE
        -O0
        -g
        --sysroot=${VESPERA_SYSROOT}
)

install(TARGETS lua ARCHIVE DESTINATION ${VESPERA_SYSROOT}/usr/lib COMPONENT runtime)