#!/bin/bash

rm cmake-build-debug -rf
mkdir cmake-build-debug
cd cmake-build-debug && cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake && cd ..

rm tests/build -rf
mkdir -p tests/build
cd tests/build && cmake ..