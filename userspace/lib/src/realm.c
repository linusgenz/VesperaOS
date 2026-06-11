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

#include <errno.h>
#include <realm.h>
#include <stdlib.h>
#include <sysstd.h>

#include "stdint.h"
#include "stdio.h"

RealmID spawn_realm(const char* path_ptr, char* const argv[], char* const envp[], spawn_config_t* cfg) {
    return sys_spawn((uint64_t)path_ptr, (uint64_t)argv, (uint64_t)envp, (uint64_t)cfg, 0, 0);
}

_Noreturn void exit(uint64_t code) {
    fflush(NULL);
    sys_exit(code, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

int64_t exit_realm(RealmID realm_id, uint64_t code) {
    return -ENOSYS;
    //  return syscall(SYSCALL_REALM_EXIT, realm_id, code, 0, 0, 0, 0);
}

int wait_realm(RealmID realm_id, int* status) {
    return (int)sys_wait(realm_id, (uintptr_t)status, 0, 0, 0, 0);
}

RealmID get_realm_id() {
    return sys_getrid(0,0,0,0,0,0);
}