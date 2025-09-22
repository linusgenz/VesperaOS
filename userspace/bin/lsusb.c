// lsusb.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 21.09.25.
//
// This file is part of VesperaOS.
// 
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include <realm.h>
#include <stdio.h>

#include "stddef.h"
#include "stdint.h"

#define MAX_INPUT 256

void _start(int argc, char **argv) {
    puts("LSUSB: ");
    printf("%s", argv[0]);

    char buf[MAX_INPUT];
    int n = fread(HANDLE_STDIN, buf, MAX_INPUT - 1);
    printf("%d", n);
    exit(0);
}
