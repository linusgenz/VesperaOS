// user_copy.cpp
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

#include "user_copy.h"

#include <klib/string.h>
#include <vespera/types.h>

namespace syscalls {
    bool is_user_range(const uptr addr, const usize len) {
        if (addr == 0) return false;
        if (len == 0) return true;
        if (addr > USER_ADDR_MAX) return false;
        if (len > USER_ADDR_MAX - addr) return false;
        return true;
    }

    bool copy_to_user(void* dst, const void* src, const usize len) {
        if (!dst || !src) return false;
        if (!is_user_range(reinterpret_cast<uptr>(dst), len)) return false;

        memcpy(dst, src, len);
        return true;
    }

    bool copy_from_user(void* dst, const void* src, const usize len) {
        if (!dst || !src) return false;
        if (!is_user_range(reinterpret_cast<uptr>(src), len)) return false;

        memcpy(dst, src, len);
        return true;
    }

    bool copy_ptr_from_user(uptr* dst, const void* src) {
        return copy_from_user(dst, src, sizeof(uptr));
    }

    bool copy_str_from_user(char* dst, const char* src, const usize max_len) {
        if (!dst || !src || max_len == 0) return false;
        if (!is_user_range(reinterpret_cast<uptr>(src), 1)) return false;

        for (usize i = 0; i < max_len; ++i) {
            // Each byte must still be within user range.
            if (!is_user_range(reinterpret_cast<uptr>(src + i), 1)) return false;

            dst[i] = src[i];

            if (dst[i] == '\0') return true;
        }

        // String did not terminate within max_len bytes, force-terminate and fail.
        dst[max_len - 1] = '\0';
        return false;
    }

    i64 copy_argv_from_user(
        const char** out_ptrs, char storage[][256], const usize max_count, const char** user_argv,
        const usize str_max_len
    ) {
        if (!out_ptrs || !storage || !user_argv) return -1;
        if (!is_user_range(reinterpret_cast<uptr>(user_argv), sizeof(uptr))) return -1;

        usize count = 0;
        while (count < max_count) {
            // Read the pointer at user_argv[count] out of user space.
            const char* user_str = nullptr;
            if (!copy_ptr_from_user(reinterpret_cast<uptr*>(&user_str), &user_argv[count])) return -1;

            if (!user_str) break;

            if (!copy_str_from_user(storage[count], user_str, str_max_len)) return -1;

            out_ptrs[count] = storage[count];
            ++count;
        }

        out_ptrs[count] = nullptr;
        return static_cast<i64>(count);
    }
} // namespace syscalls
