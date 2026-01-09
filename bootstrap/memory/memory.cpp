/**
 * @file memory.cpp
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 08.01.26.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/

#include <cstdint>
#include <cstddef>

void memset(void *dest, uint8_t val, uint64_t num) {
    for (uint64_t i = 0; i < num; i++) {
        *reinterpret_cast<uint8_t*>(reinterpret_cast<uint64_t>(dest) + i) = val;
    }
}

void *memcpy(void *dest, const void *src, size_t len) {
    auto *d = static_cast<char*>(dest);
    auto *s = static_cast<const char*>(src);
    while (len--)
        *d++ = *s++;
    return dest;
}

int memcmp(const void *ptr1, const void *ptr2, const size_t num) {
    const auto *a = static_cast<const uint8_t*>(ptr1);
    const auto *b = static_cast<const uint8_t*>(ptr2);
    for (size_t i = 0; i < num; i++) {
        if (a[i] != b[i])
            return (a[i] < b[i]) ? -1 : 1;
    }
    return 0;
}

void *memmove(void *dest, const void *src, size_t len) {
    auto *d = static_cast<char*>(dest);
    if (auto s = static_cast<const char*>(src); d < s)
        while (len--)
            *d++ = *s++;
    else {
        auto lasts = const_cast<char*>(s + (len - 1));
        auto *lastd = d + (len - 1);
        while (len--)
            *lastd-- = *lasts--;
    }
    return dest;
}