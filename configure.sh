#!/bin/bash
mkdir cmake-build-debug
cd cmake-build-debug && cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake