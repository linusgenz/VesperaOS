cmake_minimum_required(VERSION 4.1)
project(vesplib C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

file(GLOB_RECURSE LIB_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_library(vesplib STATIC ${LIB_SOURCES})

set_target_properties(vesplib PROPERTIES POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME "vesplib")

target_include_directories(vesplib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_options(vesplib PRIVATE
        -ffreestanding
        -fno-stack-protector
        -fno-builtin
        -nostdinc
        -Wall -Wextra
        -O2
        -fPIC
)