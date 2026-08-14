set(LD_COMMON_C_FLAGS
        -target x86_64-unknown-linux-gnu
        -nostdlib -nostdinc -ffreestanding
        -fno-stack-protector -fPIC -fno-plt
        -fno-asynchronous-unwind-tables
        -g -O2 -Wall -Wextra
        --sysroot=${VESPERA_SYSROOT}
        "-isystem${VESPERA_SYSROOT}/usr/include"
)

add_executable(ld_vespera
        ${CMAKE_CURRENT_SOURCE_DIR}/ld_start.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/ld_selfreloc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ld_main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ld_syscalls.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/elf_loader/elf_loader.c
)

target_compile_options(ld_vespera PRIVATE $<$<COMPILE_LANGUAGE:C>:${LD_COMMON_C_FLAGS}>)

target_include_directories(ld_vespera PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${VESPERA_SYSROOT}/usr/include"
)

set(LD_LINK_FLAGS
        "-nostdlib -target x86_64-unknown-linux-gnu -fno-stack-protector \
     -shared -fPIC -Wl,-Bsymbolic -Wl,-z,now \
     --sysroot=${VESPERA_SYSROOT}"
)

set_target_properties(ld_vespera PROPERTIES
        OUTPUT_NAME "ld-vespera"
        SUFFIX      ".so"
        LINK_FLAGS  "${LD_LINK_FLAGS}"
)

install(TARGETS ld_vespera
        DESTINATION "${VESPERA_SYSROOT}/lib"
        COMPONENT runtime)