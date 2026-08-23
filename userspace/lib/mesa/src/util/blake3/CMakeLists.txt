# CMakeLists.txt fuer blake3 - Portierung von meson.build
#
# Meson nutzt fuer x86_64/Unix vorkompilierte Assembler-Dateien (.S), nicht
# die C-Intrinsics-Dateien (blake3_sse2.c usw.) - die gibt es in diesem
# Tree fuer diesen Pfad gar nicht. Daher hier kein file(GLOB) + Filter,
# sondern explizite Auswahl analog zur cpu_family-Fallunterscheidung in
# meson.build.

set(BLAKE3_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/blake3.c
        ${CMAKE_CURRENT_SOURCE_DIR}/blake3_dispatch.c
        ${CMAKE_CURRENT_SOURCE_DIR}/blake3_portable.c
)
set(BLAKE3_DEFS "")

# --- Ziel-CPU-Familie bestimmen ---------------------------------------------
# Entspricht host_machine.cpu_family() in meson. Passe die Variable ggf. an
# das an, was euer Toolchain-File fuer VesperaOS bereits setzt
# (z.B. VESPERA_TARGET_ARCH); hier per CMAKE_SYSTEM_PROCESSOR abgeleitet.
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _blake3_arch)

set(BLAKE3_X86_NO_SIMD_DEFS
        -DBLAKE3_NO_SSE2
        -DBLAKE3_NO_SSE41
        -DBLAKE3_NO_AVX2
        -DBLAKE3_NO_AVX512
)

if(_blake3_arch MATCHES "^(x86_64|amd64)$")
    # entspricht meson: cpu_family == 'x86_64', nicht windows
    # -> Assembly-Variante fuer x86-64 Unix, kein 32-bit-pointer-Sonderfall
    #    (x32-ABI wird hier nicht unterstuetzt; falls doch benoetigt,
    #     CMAKE_SIZEOF_VOID_P pruefen wie unten kommentiert)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        # x86_64 mit 32-bit pointers (x32 ABI) - keine SIMD-Assembly
        list(APPEND BLAKE3_DEFS ${BLAKE3_X86_NO_SIMD_DEFS})
    else()
        list(APPEND BLAKE3_SOURCES
                ${CMAKE_CURRENT_SOURCE_DIR}/blake3_sse2_x86-64_unix.S
                ${CMAKE_CURRENT_SOURCE_DIR}/blake3_sse41_x86-64_unix.S
                ${CMAKE_CURRENT_SOURCE_DIR}/blake3_avx2_x86-64_unix.S
                ${CMAKE_CURRENT_SOURCE_DIR}/blake3_avx512_x86-64_unix.S
        )
        if(NOT CMAKE_ASM_COMPILER)
            # Generische ASM-Sprache braucht einen GAS-kompatiblen Treiber
            # (AT&T-Syntax, CPP-Direktiven). NASM (falls im Projekt fuer den
            # Kernel konfiguriert) ist hierfuer NICHT geeignet - die
            # blake3_*_unix.S Dateien sind GNU-Assembler-Syntax.
            set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
        endif()
    endif()
elseif(_blake3_arch MATCHES "^(x86|i386|i486|i586|i686)$")
    # entspricht meson: cpu_family == 'x86'
    # keine 32-bit-x86-Assembly vorhanden; C-Intrinsics-Varianten brauchen
    # pro Datei eigene Compile-Flags, was hier (wie in meson) bewusst
    # ausgelassen wird -> SIMD deaktiviert
    list(APPEND BLAKE3_DEFS ${BLAKE3_X86_NO_SIMD_DEFS})
elseif(_blake3_arch MATCHES "^(aarch64|arm64)$" AND CMAKE_C_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
    # entspricht meson: cpu_family == 'aarch64' and little endian
    list(APPEND BLAKE3_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/blake3_neon.c)
    list(APPEND BLAKE3_DEFS -DBLAKE3_USE_NEON=1)
endif()
# Fuer alle anderen Architekturen: kein SIMD, keine zusaetzlichen Defines
# (entspricht dem impliziten "sonst nichts tun" in meson.build)

# --- Bibliothek --------------------------------------------------------------
add_library(mesa_blake3 STATIC ${BLAKE3_SOURCES})

target_include_directories(mesa_blake3
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${VESPERA_SYSROOT}/usr/include
)

target_compile_definitions(mesa_blake3 PRIVATE ${BLAKE3_DEFS})

set_target_properties(mesa_blake3 PROPERTIES
        C_VISIBILITY_PRESET hidden
)

target_compile_options(mesa_blake3 PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wno-error>
        $<$<COMPILE_LANGUAGE:C>:-nostdinc>
        -fno-stack-protector
        -fPIC
        -O0
        -g
        --sysroot=${VESPERA_SYSROOT}
        $<$<COMPILE_LANGUAGE:C>:-isystem ${VESPERA_SYSROOT}/usr/include>
        $<$<COMPILE_LANGUAGE:C>:-idirafter ${COMPILER_BUILTIN_INCLUDE}>
)

add_dependencies(mesa_blake3 sysroot_headers)