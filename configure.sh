#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: $0 <compiler>"
    echo "Compiler must be 'clang' or 'gcc'"
    exit 1
fi

COMPILER=$1

if [ "$COMPILER" == "clang" ]; then
    CMAKE_COMPILER_FLAGS="-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_LINKER=ld.lld"
    CMAKE_CXX_FLAGS="--target=x86_64-elf -ffreestanding -nostdinc -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fshort-wchar -fno-exceptions -fno-rtti -g -O0 -fno-inline -Wall -mcmodel=kernel -fno-pic -fno-pie"
    CMAKE_C_FLAGS="--target=x86_64-elf -ffreestanding -nostdinc -fno-stack-protector -mno-red-zone -fno-pic -fno-pie -mcmodel=kernel -O2"
    CMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld"

elif [ "$COMPILER" == "gcc" ]; then
    CMAKE_COMPILER_FLAGS="-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++"
    CMAKE_CXX_FLAGS="-ffreestanding -nostdinc -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fshort-wchar -fno-exceptions -fno-rtti -g -O0 -fno-inline -Wall -mcmodel=kernel -fno-pic -fno-pie"
    CMAKE_C_FLAGS="-ffreestanding -nostdinc -fno-stack-protector -mno-red-zone -fno-pic -fno-pie -mcmodel=kernel -O2"
    CMAKE_EXE_LINKER_FLAGS="-fuse-ld=bfd"
else
    echo "Unknown compiler: $COMPILER"
    exit 1
fi

# OS Build
rm -rf cmake-build-debug
mkdir cmake-build-debug
cd cmake-build-debug
cmake .. \
    -DCMAKE_CXX_COMPILER_WORKS=TRUE \
    -DCMAKE_C_COMPILER_WORKS=TRUE \
    $CMAKE_COMPILER_FLAGS \
    -DCMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS" \
    -DCMAKE_C_FLAGS="$CMAKE_C_FLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="$CMAKE_EXE_LINKER_FLAGS"
cd ..

# Tests Build
rm -rf tests/build
mkdir -p tests/build
cd tests/build
cmake ..
cd ../..