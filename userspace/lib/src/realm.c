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

#include "stdint.h"

int64_t spawn_realm(const char* path_ptr, uint32_t argc, const char** argv) {
    return sys_spawn((uint64_t)path_ptr, argc, (uint64_t)argv, 0, 0, 0);
}

int64_t spawn_unit(uint64_t realm_id, uint64_t entry_point, uint64_t arg_ptr) {
    return -ENOSYS;
    //   return syscall(SYSCALL_UNIT_SPAWN, realm_id, entry_point, arg_ptr, 0, 0, 0);
}

__attribute__((noreturn))
void exit(uint64_t code) {
    sys_exit(code, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

int64_t exit_realm(uint64_t realm_id, uint64_t code) {
    return -ENOSYS;
    //  return syscall(SYSCALL_REALM_EXIT, realm_id, code, 0, 0, 0, 0);
}
