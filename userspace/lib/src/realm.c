// realm.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 22.09.25.
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

#include <sysstd.h>
#include <errno.h>
#include <realm.h>
#include <stdlib.h>
#include "stdint.h"

RealmID spawn_realm(const char* path_ptr, char *const argv[], char *const envp[])
{
    return sys_spawn((uint64_t)path_ptr, (uint64_t)argv, (uint64_t)envp, 0, 0, 0);
}

UnitID spawn_unit(RealmID realm_id, uint64_t entry_point, uint64_t arg_ptr)
{
    return -ENOSYS;
    //   return syscall(SYSCALL_UNIT_SPAWN, realm_id, entry_point, arg_ptr, 0, 0, 0);
}

__attribute__((noreturn)) void exit(uint64_t code)
{
    sys_exit(code, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

int64_t exit_realm(RealmID realm_id, uint64_t code)
{
    return -ENOSYS;
    //  return syscall(SYSCALL_REALM_EXIT, realm_id, code, 0, 0, 0, 0);
}

int wait_realm(RealmID realm_id, int* status)
{
    return (int)sys_wait(realm_id,
                         (uintptr_t)status,
                         0, 0, 0, 0);
}
