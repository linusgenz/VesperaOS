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
#include <cstdint>
#include <log.h>
#include "disasm.h"
#include "symbols.h"

void debug_capture_current_stack(uint64_t *out, uint8_t *out_len, uint8_t max_depth) {
    uint64_t *rbp;
    asm volatile ("mov %%rbp, %0" : "=r" (rbp));
    uint8_t cnt = 0;
    while (rbp && cnt < max_depth) {
        if ((uintptr_t) rbp & 0xF) break;
        uint64_t ret = *(rbp + 1);
        if (!ret) break;
        out[cnt++] = ret;

        uint64_t next_rbp = *rbp;
        if (next_rbp <= (uintptr_t) rbp) break;
        rbp = (uint64_t *) next_rbp;
    }
    *out_len = cnt;
}

void disassemble_frame(uint64_t addr, size_t bytes) {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(addr);
    size_t offset = 0;

    while (offset < bytes) {
        Instruction ins = disasm_next(ptr + offset, bytes - offset);
        Log::PrintLn("    %p: %s", ptr + offset, ins.mnemonic);
        offset += ins.size;
    }
}

void backtrace() {
    uint64_t frames[32];
    uint8_t count = 0;

    debug_capture_current_stack(frames, &count, 32);

    Log::PrintLn("Backtrace (frames: %u, most recent call last)", count);

    for (uint8_t i = 0; i < count; i++) {
        Symbol s = lookup_symbol(frames[i]);
        uint64_t offset = frames[i] - s.addr;

        Log::PrintLn("  #%u  %p  <%.*s+0x%llx>",
                     i,
                     (void *) frames[i],
                     (int) s.len,
                     s.name,
                     offset);

        if (i == 0) {
            disassemble_frame(frames[i], 16);
        }
    }
}
