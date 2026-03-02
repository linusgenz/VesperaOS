#!/usr/bin/env bash

cmake -S . -B build && cmake --build build

echo
echo "========= FAT32 TEST ========="
echo

./build/fat32_tests
if [ $? -eq 0 ]; then
    echo "Tests successful"
else
    echo "Tests failed"
fi