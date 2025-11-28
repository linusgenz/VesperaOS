set(TARGET "x86_64-vespera-elf")
set(PREFIX "/usr/local/cross")

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
set(CMAKE_C_STANDARD_LIBRARIES "")
set(CMAKE_CXX_STANDARD_LIBRARIES "")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER "${PREFIX}/bin/${TARGET}-gcc")
set(CMAKE_CXX_COMPILER "${PREFIX}/bin/${TARGET}-g++")
set(CMAKE_ASM_COMPILER "${PREFIX}/bin/${TARGET}-gcc")

set(CMAKE_AR             "${PREFIX}/bin/${TARGET}-ar")
set(CMAKE_RANLIB         "${PREFIX}/bin/${TARGET}-ranlib")
set(CMAKE_OBJDUMP        "${PREFIX}/bin/${TARGET}-objdump")
set(CMAKE_OBJCOPY        "${PREFIX}/bin/${TARGET}-objcopy")
set(CMAKE_NM             "${PREFIX}/bin/${TARGET}-nm")
