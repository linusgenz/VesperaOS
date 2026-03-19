// trace.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 20.11.25.
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

#include "trace.h"

#include "symbols.h"
#include <vespera/log.h>

void debug_capture_stack(u64 rbp, u64 rip, u64* out, u8* out_len, u8 max_depth) {
    u8 cnt = 0;

    out[cnt++] = rip;

    while (rbp && cnt < max_depth) {
        if (rbp & 0xF) break;

        const u64 ret = *reinterpret_cast<u64*>(rbp + 8);
        if (!ret) break;

        out[cnt++] = ret;

        const u64 next_rbp = *reinterpret_cast<u64*>(rbp);
        if (next_rbp <= rbp) break;

        rbp = next_rbp;
    }

    *out_len = cnt;
}

void backtrace(u64 rbp_start, u64 rip_start) {
    u64 frames[32];
    u8 count = 0;

    debug_capture_stack(rbp_start, rip_start, frames, &count, 32);

    Log::print_ln("Backtrace (frames: %u, most recent call last)", count);

    for (u8 i = 0; i < count; i++) {
        Symbol s = lookup_symbol(frames[i]);
        const u64 offset = frames[i] - s.addr;

        Log::print_ln(
            "  #%u  %p  <%.*s+0x%llx>", i, reinterpret_cast<void*>(frames[i]), static_cast<int>(s.len), s.name, offset
        );

        //  if (i == 0)
        //      disassemble_frame(frames[i], 60);
    }
}