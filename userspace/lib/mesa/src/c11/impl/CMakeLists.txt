# lib/mesa/src/util/c11/CMakeLists.txt
#
# Portierung von src/util/c11/meson.build.
#
# with_c11_threads-Verzweigung fest auf den POSIX-Zweig gelegt:
# VesperaOS hat einen eigenen POSIX-kompatiblen pthread-Layer
# (Mutexes, Condvars, Barriers, pthread_once, RW-Locks, TSS, Thread-
# Erstellung/Join via Trampolin), aber noch keine native C11
# <threads.h>-Implementierung in der eigenen libc. Daher: immer
# threads_posix.c, kein with_c11_threads-Fallzweig, kein Windows-
# Zweig (threads_win32.c/.cpp entfaellt komplett -- kein Windows-
# Target).
#
# Falls ihr spaeter <threads.h> nativ in eurer libc implementiert:
# dann wuerde dieser gesamte Layer entfallen (Mesa nutzt dann direkt
# die C11-Standardfunktionen, keine Wrapper-Uebersetzung mehr noetig)
# -- nicht einfach "still switchen", das muss explizit entschieden
# werden, da idep_mesautilc11 dann im Mesa-Baum ueberhaupt nicht mehr
# als Dependency eingebunden wird.

set(UTIL_C11_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(MESA_UTIL_C11_SOURCES
        ${UTIL_C11_DIR}/time.c
        ${UTIL_C11_DIR}/threads_posix.c
)

add_library(mesa_util_c11 STATIC ${MESA_UTIL_C11_SOURCES})

# inc_util_c11 -- meson.build: include_directories('../..'), relativ
# zu src/util/c11 aufgeloest ergibt das src/. Wir geben MESA_ROOT/src
# direkt an, um nicht von der relativen Verzeichnistiefe abzuhaengen.
target_include_directories(mesa_util_c11
        PUBLIC
        ${MESA_ROOT}/src
        PRIVATE
        ${VESPERA_SYSROOT}/usr/include
)

target_compile_options(mesa_util_c11 PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${MESA_COMMON_C_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${MESA_COMMON_CXX_FLAGS}>
)

target_compile_options(mesa_util_c11 PRIVATE -fvisibility=hidden)

target_compile_definitions(mesa_util_c11 PUBLIC HAVE_PTHREAD HAVE_STRUCT_TIMESPEC)

add_dependencies(mesa_util_c11 sysroot_headers)
