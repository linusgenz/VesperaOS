# userspace/lib/vespera-gl-offscreen/CMakeLists.txt

set(MESA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../mesa)
set(MESA_BUILD_ROOT ${CMAKE_CURRENT_BINARY_DIR}/../mesa_build/)

add_library(vespera_gl_offscreen STATIC
        vespera_gl_offscreen.c
)

add_compile_definitions(
        HAVE_ENDIAN_H=1
        HAVE_PTHREAD
        HAVE_STRUCT_TIMESPEC
)

target_include_directories(vespera_gl_offscreen
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE
        ${MESA_ROOT}/include
        ${MESA_ROOT}/src
        ${MESA_ROOT}/src/gallium/include
        ${MESA_ROOT}/src/gallium/auxiliary
        ${MESA_ROOT}/src/mesa
        ${MESA_BUILD_ROOT}/src/util/gen
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(vespera_gl_offscreen PRIVATE
        ${MESA_COMMON_C_FLAGS}
)

target_link_libraries(vespera_gl_offscreen PUBLIC mesa)
add_dependencies(vespera_gl_offscreen mesa)