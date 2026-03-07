#!/bin/bash

rm cmake-build-debug -rf
mkdir cmake-build-debug
cd cmake-build-debug && cmake .. -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_LINKER=ld.lld && cd ..

rm tests/build -rf
mkdir -p tests/build
cd tests/build && cmake ..