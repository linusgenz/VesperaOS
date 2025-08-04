// syscall_interface.h
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

#ifndef SYSCALL_INTERFACE_H
#define SYSCALL_INTERFACE_H

#include <stdint.h>

namespace syscalls::internal {
    using syscall_fn = int64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

    int64_t sys_write(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

    int64_t sys_exit(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

    int64_t sys_read(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

    int64_t sys_close(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

    int64_t sys_open(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

    int64_t sys_create(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

    int64_t sys_rename(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

    int64_t sys_mkdir(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

    int64_t sys_rmdir(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

    int64_t sys_unlink(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
}

void install_syscalls();

#endif //SYSCALL_INTERFACE_H
