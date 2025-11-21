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

void debug_capture_current_stack(uint64_t* out, uint8_t* out_len, uint8_t max_depth) {
    uint64_t *rbp;
    asm volatile ("mov %%rbp, %0" : "=r" (rbp));
    uint8_t cnt = 0;
    while (rbp && cnt < max_depth) {
        uint64_t ret = *(rbp + 1);
        if (!ret) break;
        out[cnt++] = ret;
        rbp = (uint64_t*)(*rbp);
    }
    *out_len = cnt;
}
