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

#include <vespera/types.h>

namespace syscalls::internal {
    using syscall_fn_t = i64 (*)(u64, u64, u64, u64, u64, u64);

    i64 sys_write(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64);

    i64 sys_exit(u64 arg0, u64, u64, u64, u64, u64);

    i64 sys_read(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64);

    i64 sys_close(u64 arg0, u64, u64, u64, u64, u64);

    i64 sys_open(u64 arg0, u64, u64, u64, u64, u64);

    i64 sys_create(u64 arg0, u64, u64, u64, u64, u64);

    i64 sys_rename(u64 arg0, u64 arg1, u64, u64, u64, u64);

    i64 sys_mkdir(u64 arg0, u64, u64, u64, u64, u64);

    i64 sys_rmdir(u64 arg0, u64, u64, u64, u64, u64);

    i64 sys_unlink(u64 arg0, u64, u64, u64, u64, u64);

    i64 sys_reboot(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64);

    i64 sys_sleep(u64 arg0, u64, u64, u64, u64, u64);

    i64 sys_ioctl(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64);

    i64 sys_spawn(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64);

    i64 sys_readdir(u64 arg0, u64 arg1, u64, u64, u64, u64);

    i64 sys_wait(u64 arg0, u64 arg1, u64, u64, u64, u64);

    i64 sys_mmap(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5);

    i64 sys_munmap(u64 arg0, u64 arg1, u64, u64, u64, u64);

    i64 sys_brk(u64 addr, u64, u64, u64, u64, u64);

    i64 sys_channel_create(u64 arg0, u64, u64, u64, u64, u64);

    i64 sys_channel_send(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64);

    i64 sys_channel_recv(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64);

    i64 sys_seek(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64);

    i64 sys_chdir(u64 arg0, u64, u64, u64, u64, u64);

    i64 sys_getcwd(u64 arg0, u64 arg1, u64, u64, u64, u64);

    i64 sys_stat(u64 arg0, u64 arg1, u64, u64, u64, u64);

    i64 sys_poll(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64);

    i64 sys_pipe(u64 arg0, u64, u64, u64, u64, u64);

    i64 sys_getrid(u64, u64, u64, u64, u64, u64);

    i64 sys_getuid(u64, u64, u64, u64, u64, u64);

    i64 sys_mount(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64);

    i64 sys_umount(u64 arg0, u64 arg1, u64, u64, u64, u64);
}  // namespace syscalls::internal

void install_syscalls();

#endif  // SYSCALL_INTERFACE_H
