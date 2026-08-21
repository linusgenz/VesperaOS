# lib/mesa/CMakeLists.txt
#
# Root-Wrapper fuer den vendorten Mesa-Baum. Baut NICHT mit Mesa's eigenem
# Meson-Buildsystem (siehe Begruendung in der Projekt-Historie: Mesons
# Feature-Detection ist zu eng an POSIX/Linux gekoppelt fuer ein OS mit
# eigener, im Aufbau befindlicher libc). Stattdessen orchestrieren wir
# Mesa's unveraendertete Quellstruktur mit eigenem CMake, Layer fuer Layer,
# in Abhaengigkeitsreihenfolge:
#
#   util -> compiler/nir,glsl,spirv -> intel/dev -> intel/compiler
#        -> intel/isl -> intel/common,blorp -> gallium/auxiliary
#        -> iris (driver) -> iris (winsys) -> gallium/frontends/mesa
#        -> mesa/main -> mapi -> finales libGL
#
# Aktueller Stand: nur der erste Layer (util) ist eingehaengt. Weitere
# add_subdirectory()-Aufrufe werden Schritt fuer Schritt ergaenzt, sobald
# der jeweils vorherige Layer sauber durchkompiliert.

set(MESA_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" MESA_VERSION)
add_compile_definitions(PACKAGE_VERSION="${MESA_VERSION}")

set(MESA_COMMON_C_FLAGS
        -target x86_64-vespera-elf
        -nostdinc
        -nostdinc++
        -fno-stack-protector
        -Wno-initializer-overrides
        -fPIC
        -O2
        -g
        --sysroot=${VESPERA_SYSROOT}
        -isystem${VESPERA_SYSROOT}/usr/include
        -isystem${VESPERA_SYSROOT}/usr/include/c++
        -idirafter ${COMPILER_BUILTIN_INCLUDE}
)

include(${CMAKE_CURRENT_SOURCE_DIR}/FindMesaPython.cmake)
include(ExternalProject)
ExternalProject_Add(host-tools
        SOURCE_DIR          "${CMAKE_CURRENT_SOURCE_DIR}/host-tools"

        CMAKE_ARGS          -DMESA_SOURCE_ROOT=${MESA_ROOT}
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=Release

        DOWNLOAD_COMMAND    ""
        UPDATE_COMMAND      ""

        BUILD_BYPRODUCTS    <INSTALL_DIR>/bin/mesa_clc
        <INSTALL_DIR>/bin/vtn_bindgen2
)

ExternalProject_Get_Property(host-tools install_dir)

set(MESA_CLC_EXECUTABLE ${install_dir}/bin/mesa_clc)
set(VTN_BINDGEN2_EXECUTABLE ${install_dir}/bin/vtn_bindgen2)

add_subdirectory(src/c11/impl)
add_subdirectory(src/util)
add_subdirectory(src/compiler)
add_subdirectory(src/intel/genxml)
add_subdirectory(src/intel/dev)
add_subdirectory(src/intel/decoder)
add_subdirectory(src/intel/isl)
add_subdirectory(src/intel/common)
add_subdirectory(src/intel/compiler)
add_subdirectory(src/intel/shaders)
# add_subdirectory(src/intel/perf)
add_subdirectory(src/intel/tools)
add_subdirectory(src/intel/blorp)
add_subdirectory(src/intel/ds)
add_subdirectory(src/gallium/auxiliary)
add_subdirectory(src/gallium/drivers/iris)
add_subdirectory(src/mesa)
# add_subdirectory(src/gallium/frontends/mesa)
# add_subdirectory(src/mesa/main)


add_custom_target(mesa_root
        DEPENDS
        mesa_util
        mesa_genxml
        mesa_compiler
        mesa_nir
        mesa_intel_dev
        mesa_intel_compiler
        mesa_intel_blorp
        mesa_intel_decoder
        mesa_intel_ds
#        mesa_intel_perf
        mesa_intel_shaders
        mesa_gallium_aux
        mesa_iris_driver
        glapi
        GLESv2
        mesa
        COMMENT "Building all wired Mesa layers"
)

add_dependencies(mesa_root host-tools)
