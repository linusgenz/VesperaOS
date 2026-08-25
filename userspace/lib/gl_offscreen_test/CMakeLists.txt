# gl_offscreen_test/CMakeLists.txt

set(MESA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../mesa)
set(MESA_BUILD_ROOT ${CMAKE_CURRENT_BINARY_DIR}/../mesa_build/)

add_executable(vespera_gl_offscreen_test
        vespera_gl_offscreen_test.c
        $<TARGET_OBJECTS:crt0_obj>
)

add_compile_definitions(
        HAVE_ENDIAN_H=1
        HAVE_PTHREAD
        HAVE_STRUCT_TIMESPEC
)

target_link_libraries(vespera_gl_offscreen_test PRIVATE
        vespera_gl_offscreen
        GLESv2
        vesplib
        vesplib_cpp
)

target_include_directories(vespera_gl_offscreen_test PRIVATE
        ${VESPERA_SYSROOT}/usr/include
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/src/gallium/auxiliary
        ${MESA_ROOT}/src/mesa
        ${MESA_BUILD_ROOT}/src/util/gen
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(vespera_gl_offscreen_test PRIVATE
        -target x86_64-vespera-elf
        -O2
        -fno-omit-frame-pointer
        -fPIE
        -ftls-model=initial-exec
        --sysroot=${VESPERA_SYSROOT}
        -fno-stack-protector
)

target_link_options(vespera_gl_offscreen_test PRIVATE
        -nostdlib
        -pie
        -fno-stack-protector
        "-Wl,-dynamic-linker,/lib/ld-vespera.so"
)

install(TARGETS vespera_gl_offscreen_test RUNTIME DESTINATION ${VESPERA_SYSROOT}/bin COMPONENT runtime)