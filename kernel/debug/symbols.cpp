// symbols.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 24.11.25.
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

#include "symbols.h"
#include <string.h>
#include <kernel_symbols.h>

static bool parse_hex_u64(const char* s, uint64_t* out)
{
    uint64_t v = 0;
    while (*s && isxdigit(*s)) {
        v = (v << 4)
            | (uint64_t)(isdigit(*s) ? (*s - '0') : (tolower(*s) - 'a' + 1 + 9));
        s++;
    }
    *out = v;
    return true;
}

Symbol lookup_symbol(uint64_t addr)
{
    auto best = "???";
    size_t best_len = 3;
    uint64_t best_addr = 0;

    auto p = reinterpret_cast<const char*>(kernel_map);
    const char* end = p + kernel_map_len;

    while (p < end) {
        const char* line = p;

        uint64_t sym_addr;
        if (!parse_hex_u64(line, &sym_addr)) {
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
            continue;
        }

        const char* type = strchr(line, ' ');
        if (!type) {
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
            continue;
        }

        type++;
        if (*type != 'T' && *type != 't') {
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
            continue;
        }

        const char* name = type + 2;
        const char* line_end = strchr(name, '\n');
        size_t len = line_end ? (size_t)(line_end - name) : (size_t)(end - name);

        if (sym_addr <= addr && sym_addr > best_addr) {
            best_addr = sym_addr;
            best = name;
            best_len = len;
        }

        while (p < end && *p != '\n') p++;
        if (p < end) p++;
    }

    return { best, best_len, best_addr };
}

