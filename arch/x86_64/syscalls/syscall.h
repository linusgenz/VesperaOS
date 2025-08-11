// syscall.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.08.25.
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

#ifndef SYSCALL_H
#define SYSCALL_H
#include "stdint.h"


void syscall_init();

int64_t syscall(
    uint64_t num,
    uint64_t arg0 = 0,
    uint64_t arg1 = 0,
    uint64_t arg2 = 0,
    uint64_t arg3 = 0,
    uint64_t arg4 = 0,
    uint64_t arg5 = 0
);

#endif //SYSCALL_H
