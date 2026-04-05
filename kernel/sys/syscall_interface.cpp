// syscall_interface.cpp
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

#include "syscall_interface.h"

#include <vespera/log.h>
#include <vespera/scheduling.h>
#include <vespera/signals.h>
#include <vespera/sys/syscall_numbers.h>

constexpr int MAX_SYSCALLS = 512;
static syscalls::internal::syscall_fn_t syscall_table[MAX_SYSCALLS];

void install_syscalls() {
    for (auto& i : syscall_table) {
        i = nullptr;
    }

    syscall_table[SYSCALL_READ] = syscalls::internal::sys_read;
    syscall_table[SYSCALL_WRITE] = syscalls::internal::sys_write;
    syscall_table[SYSCALL_EXIT] = syscalls::internal::sys_exit;
    syscall_table[SYSCALL_CLOSE] = syscalls::internal::sys_close;
    syscall_table[SYSCALL_OPEN] = syscalls::internal::sys_open;
    syscall_table[SYSCALL_CREATE] = syscalls::internal::sys_create;
    syscall_table[SYSCALL_RENAME] = syscalls::internal::sys_rename;
    syscall_table[SYSCALL_MKDIR] = syscalls::internal::sys_mkdir;
    syscall_table[SYSCALL_RMDIR] = syscalls::internal::sys_rmdir;
    syscall_table[SYSCALL_UNLINK] = syscalls::internal::sys_unlink;
    syscall_table[SYSCALL_REBOOT] = syscalls::internal::sys_reboot;
    syscall_table[SYSCALL_SLEEP] = syscalls::internal::sys_sleep;
    syscall_table[SYSCALL_IOCTL] = syscalls::internal::sys_ioctl;
    syscall_table[SYSCALL_SPAWN] = syscalls::internal::sys_spawn;
    syscall_table[SYSCALL_READDIR] = syscalls::internal::sys_readdir;
    syscall_table[SYSCALL_WAIT] = syscalls::internal::sys_wait;
    syscall_table[SYSCALL_MMAP] = syscalls::internal::sys_mmap;
    syscall_table[SYSCALL_MUNMAP] = syscalls::internal::sys_munmap;
    syscall_table[SYSCALL_BRK] = syscalls::internal::sys_brk;
    syscall_table[SYSCALL_CHANNEL_CREATE] = syscalls::internal::sys_channel_create;
    syscall_table[SYSCALL_CHANNEL_SEND] = syscalls::internal::sys_channel_send;
    syscall_table[SYSCALL_CHANNEL_RECEIVE] = syscalls::internal::sys_channel_recv;
    syscall_table[SYSCALL_SEEK] = syscalls::internal::sys_seek;
    syscall_table[SYSCALL_CHDIR] = syscalls::internal::sys_chdir;
    syscall_table[SYSCALL_GETCWD] = syscalls::internal::sys_getcwd;
    syscall_table[SYSCALL_STAT] = syscalls::internal::sys_stat;
    syscall_table[SYSCALL_POLL] = syscalls::internal::sys_poll;
    syscall_table[SYSCALL_PIPE] = syscalls::internal::sys_pipe;
    syscall_table[SYSCALL_GETRID] = syscalls::internal::sys_getrid;
    syscall_table[SYSCALL_GETUID] = syscalls::internal::sys_getuid;
    syscall_table[SYSCALL_MOUNT] = syscalls::internal::sys_mount;
    syscall_table[SYSCALL_UMOUNT] = syscalls::internal::sys_umount;
    syscall_table[SYSCALL_SIGACTION] = syscalls::internal::sys_sigaction;
    syscall_table[SYSCALL_SIGRETURN] = syscalls::internal::sys_sigreturn;
    syscall_table[SYSCALL_KILL] = syscalls::internal::sys_kill;
    syscall_table[SYSCALL_CLOCK_GETTIME] = syscalls::internal::sys_clock_gettime;
    syscall_table[SYSCALL_VBUS_SUBSCRIBE] = syscalls::internal::sys_vbus_subscribe;
    syscall_table[SYSCALL_VBUS_UNSUBSCRIBE] = syscalls::internal::sys_vbus_unsubscribe;
}

extern "C" i64 syscall_handler(u64 num, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    u64 ret = 0;

    if (num < MAX_SYSCALLS && syscall_table[num]) [[likely]] {
        asm volatile("sti");
        ret = syscall_table[num](arg0, arg1, arg2, arg3, arg4, arg5);
    } else {
        Log::print_ln("[SYSCALL] Invalid syscall number: %u", num);
    }

    Unit* u = kernel::scheduling::get_current_unit();
    if (u && u->is_user) {
        TrapFrame* trap = &u->context.current_trap_frame;
        trap->rax = ret;
        signal_dispatch(u, trap);
    }

    return ret;
}
