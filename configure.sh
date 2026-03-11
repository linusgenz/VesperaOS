#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: $0 <compiler>"
    echo "Compiler must be 'clang' or 'gcc'"
    exit 1
fi

COMPILER=$1

if [ "$COMPILER" == "clang" ]; then
    CMAKE_COMPILER_FLAGS="-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_LINKER=ld.lld"
    CMAKE_CXX_FLAGS="--target=x86_64-elf -ffreestanding -nostdinc -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fshort-wchar -fno-exceptions -fno-rtti -g -O0 -fno-inline -Wall -mcmodel=kernel -mno-sse -fno-pic -fno-pie"
elif [ "$COMPILER" == "gcc" ]; then
    CMAKE_COMPILER_FLAGS=""
    CMAKE_CXX_FLAGS="-ffreestanding -nostdinc -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fshort-wchar -fno-exceptions -fno-rtti -g -O0 -fno-inline -Wall -mcmodel=kernel -mno-sse -fno-pic -fno-pie"
else
    echo "Unknown compiler: $COMPILER"
    exit 1
fi

# OS Build
rm -rf cmake-build-debug
mkdir cmake-build-debug
cd cmake-build-debug
cmake .. -DCMAKE_CXX_COMPILER_WORKS=TRUE $CMAKE_COMPILER_FLAGS -DCMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS"
cd ..

# Tests Build
rm -rf tests/build
mkdir -p tests/build
cd tests/build
cmake ..
cd ../..