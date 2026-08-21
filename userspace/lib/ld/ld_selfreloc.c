// ld_selfreloc.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 13.08.26.
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

#include <stdint.h>

#include "elf.h"

#define SR_R_X86_64_RELATIVE 8

void ld_selfreloc_apply(uint64_t load_bias, const Elf64_Dyn* dyn_runtime) {
    uint64_t rela_addr = 0;
    uint64_t rela_size = 0;
    uint64_t rela_ent = sizeof(Elf64_Rela);

    for (int i = 0; dyn_runtime[i].d_tag != DT_NULL; i++) {
        switch (dyn_runtime[i].d_tag) {
            case DT_RELA:
                rela_addr = dyn_runtime[i].d_un.d_ptr + load_bias;
                break;
            case DT_RELASZ:
                rela_size = dyn_runtime[i].d_un.d_val;
                break;
            case DT_RELAENT:
                rela_ent = dyn_runtime[i].d_un.d_val;
                break;
            default:
                break;
        }
    }

    if (!rela_addr || !rela_size || !rela_ent) {
        return;
    }

    uint64_t count = rela_size / rela_ent;
    Elf64_Rela* relocs = (Elf64_Rela*)rela_addr;

    for (uint64_t i = 0; i < count; i++) {
        Elf64_Rela* r = &relocs[i];
        uint32_t type = (uint32_t)(r->r_info & 0xffffffffu);

        // Strict prohibition: everything except RELATIVE is ignored.
        if (type != SR_R_X86_64_RELATIVE) {
            continue;
        }

        uint64_t* target = (uint64_t*)(r->r_offset + load_bias);
        *target = load_bias + (uint64_t)r->r_addend;
    }
}
