/**
 * @file bootstrap.h
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
#ifndef VESPERAOS_BOOTSTRAP_H
#define VESPERAOS_BOOTSTRAP_H
#include <cstdint>
#include <cstddef>
#include <memory.h>
#include <graphics.h>

extern uint64_t _PhysStart;
extern uint64_t _PhysEnd;

struct BootInfo
{
    Framebuffer* fb;
    FONT* font;
    EFI_MEMORY_DESCRIPTOR* mmap;
    uint64_t mmap_size;
    uint64_t mmap_desc_size;
    void* rsdp;
};

typedef void (*KernelEntry)(BootInfo*);

struct BootstrapInfo {
    BootInfo bi;
    void* phys_start;
    uint64_t phys_size;
    KernelEntry kernel_entry;
};

#endif //VESPERAOS_BOOTSTRAP_H