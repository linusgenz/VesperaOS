// user_copy.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 01.06.26.
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
#ifndef VESPERAOS_USER_COPY_H
#define VESPERAOS_USER_COPY_H

#include <vespera/types.h>

namespace syscalls {

    // Upper bound of the canonical user-space virtual address range (x86_64).
    static constexpr uptr USER_ADDR_MAX = 0x0000'7FFF'FFFF'FFFFull;

    // Returns true if the entire range [addr, addr+len) lies within user space.
    [[nodiscard]] bool is_user_range(uptr addr, usize len);

    // Copies `len` bytes from the user-space address `src` into the kernel
    // buffer `dst`. Returns false if `src` is not a valid user-space address
    // or if the range is otherwise inaccessible.
    [[nodiscard]] bool copy_from_user(void* dst, const void* src, usize len);

    bool copy_to_user(void* dst, const void* src, usize len);

    // Reads a single pointer-sized value from user space.
    // Convenience wrapper around copy_from_user.
    [[nodiscard]] bool copy_ptr_from_user(uptr* dst, const void* src);

    // Copies a null-terminated string from the user-space address `src` into
    // the kernel buffer `dst` (capacity `max_len` including the null terminator).
    // Returns false if `src` is not a valid user-space address, if the string
    // exceeds `max_len - 1` characters, or if the range is inaccessible.
    [[nodiscard]] bool copy_str_from_user(char* dst, const char* src, usize max_len);

    // Copies a null-terminated array of string pointers (argv / envp style)
    // from user space into `out_ptrs` (capacity `max_count` + implicit nullptr).
    // Each string is copied into the corresponding slot of `storage`
    // (each slot has capacity `str_max_len`).
    // Returns the number of strings copied, or -1 on error.
    i64 copy_argv_from_user(
        const char** out_ptrs, char storage[][256], usize max_count, const char** user_argv, usize str_max_len = 256
    );

}  // namespace syscalls

#endif  // VESPERAOS_USER_COPY_H
