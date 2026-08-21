# lib/mesa/src/intel/mda/CMakeLists.txt

set(MDA_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(MESA_INTEL_MDA_SOURCES
        debug_archiver.c
        mda_match.c
        slice.c
        tar.c
)

add_library(mesa_intel_mda STATIC ${MESA_INTEL_MDA_SOURCES})

target_include_directories(mesa_intel_mda
        PUBLIC
        ${MDA_DIR}
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_link_libraries(mesa_intel_mda
        PUBLIC
        mesa_util
)

target_compile_options(mesa_intel_mda PRIVATE ${MESA_COMMON_C_FLAGS})

add_dependencies(mesa_intel_mda sysroot_headers)