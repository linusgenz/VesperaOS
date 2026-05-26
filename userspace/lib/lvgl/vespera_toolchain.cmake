# Bezeichne das Ziel-System (verhindert, dass CMake Linux-Annahmen trifft)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Pfad zu deinem Sysroot (Passe den Pfad an dein System an!)
set(SYSROOT "/mnt/ExternerDatentraeger/VesperaOS/sysroot")
set(CMAKE_SYSROOT ${SYSROOT})

# Compiler definieren
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Flags, damit für ein eigenes OS ohne Standard-Standardbibliothek gebaut wird
set(VESPERA_FLAGS "-ffreestanding -nostdlib -nostdinc -fno-stack-protector -fPIE")

set(CMAKE_C_FLAGS "${VESPERA_FLAGS} -isystem ${SYSROOT}/usr/include" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${VESPERA_FLAGS} -isystem ${SYSROOT}/usr/include" CACHE STRING "" FORCE)

# Such-Verhalten von CMake anpassen (soll nur im Sysroot suchen, nicht auf dem Host-PC)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
