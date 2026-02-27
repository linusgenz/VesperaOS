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

#include <kernel/memory.h>

#include "../../include/log.h"
#include <kernel/scheduling.h>
#include <kernel/sys/syscall_numbers.h>
#include <kernel/realm/realm_manager.h>

constexpr int MAX_SYSCALLS = 256;
static syscalls::internal::syscall_fn syscall_table[MAX_SYSCALLS];

void install_syscalls() {
    for (auto & i : syscall_table) {
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
}

extern "C" void syscall_handler(
    uint64_t num,
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    uint64_t ret = 0;

 //   asm volatile("mov %0, %%cr3" :: "r"(kernel::memory::get_pagetable_address()));

    if (num < MAX_SYSCALLS && syscall_table[num]) {
        asm volatile("sti");
        ret = syscall_table[num](arg0, arg1, arg2, arg3, arg4, arg5);
    } else {
        Log::PrintLn("[SYSCALL] Invalid syscall number: %u", num);
    }

   /* Realm *r = RealmManager::get(kernel::scheduling::get_current_unit()->rid);
    uint64_t cr3 = r->pml4_phys;
    asm volatile("mov %0, %%cr3" :: "r"(cr3));*/

    asm volatile ("mov %0, %%rax" :: "r"(ret));
}
