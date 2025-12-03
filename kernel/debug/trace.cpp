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
#include <log.h>
#include "disasm.h"
#include "symbols.h"

void debug_capture_stack(uint64_t rbp, uint64_t rip,
                              uint64_t *out, uint8_t *out_len,
                              uint8_t max_depth) {

    uint8_t cnt = 0;

    out[cnt++] = rip;

    while (rbp && cnt < max_depth) {

        if (rbp & 0xF) break;

        uint64_t ret = *reinterpret_cast<uint64_t*>(rbp + 8);
        if (!ret) break;

        out[cnt++] = ret;

        uint64_t next_rbp = *reinterpret_cast<uint64_t*>(rbp);
        if (next_rbp <= rbp) break; 

        rbp = next_rbp;
    }

    *out_len = cnt;
}

void backtrace(uint64_t rbp_start, uint64_t rip_start) {
    uint64_t frames[32];
    uint8_t count = 0;

    debug_capture_stack(rbp_start, rip_start, frames, &count, 32);

    Log::PrintLn("Backtrace (frames: %u, most recent call last)", count);

    for (uint8_t i = 0; i < count; i++) {
        Symbol s = lookup_symbol(frames[i]);
        uint64_t offset = frames[i] - s.addr;

        Log::PrintLn("  #%u  %p  <%.*s+0x%llx>",
                     i,
                     reinterpret_cast<void*>(frames[i]),
                     static_cast<int>(s.len),
                     s.name,
                     offset);

        if (i == 0)
            disassemble_frame(frames[i], 60);
    }
}