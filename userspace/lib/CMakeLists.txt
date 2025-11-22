cmake_minimum_required(VERSION 4.1)
project(VesperaLib C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB LIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_library(vespera STATIC ${LIB_SOURCES})

target_compile_options(vespera PRIVATE
    -ffreestanding
    -fno-stack-protector
    -fno-builtin
    -nostdinc
    -Wall -Wextra -O2
)

set_target_properties(vespera PROPERTIES OUTPUT_NAME "vespera")

