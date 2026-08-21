# lib/mesa/src/util/blake3/CMakeLists.txt
#
# Entspricht Mesa upstream src/util/blake3/meson.build (idep_blake3).
# Reine C-Hashing-Bibliothek, keine externen Abhaengigkeiten -- vermutlich
# der einfachste Layer im gesamten Baum. Falls SIMD-Varianten (avx2/sse etc.)
# in eurem kopierten Verzeichnis vorhanden sind, hier bei Bedarf ergaenzen;
# fuer den ersten Build reicht die portable C-Referenzimplementierung.

file(GLOB BLAKE3_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

# Falls SIMD-spezifische Dateien (blake3_sse2.c, blake3_avx2.c, etc.)
# vorhanden sind und noch nicht laufen sollen, hier gezielt rausfiltern:
list(FILTER BLAKE3_SOURCES EXCLUDE REGEX ".*_(sse2|sse41|avx2|avx512|neon)\\.c$")

add_library(mesa_blake3 STATIC ${BLAKE3_SOURCES})

target_include_directories(mesa_blake3
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_blake3 PRIVATE
        -nostdinc
        -fno-stack-protector
        -fPIC
        -O0
        -g
        --sysroot=${VESPERA_SYSROOT}
        -isystem ${VESPERA_SYSROOT}/usr/include
        -idirafter ${COMPILER_BUILTIN_INCLUDE}
)

add_dependencies(mesa_blake3 sysroot_headers)