// boot.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 15.11.25.
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

#ifndef VESPERAOS_BOOT_H
#define VESPERAOS_BOOT_H

#include <acpi/acpi.h>
#include <vespera/graphics/psf.h>
#include <vespera/graphics/fb.h>
#include <vespera/mm/efi_memory.h>

struct BootInfo {
    Framebuffer*           framebuffer;
    PsfFont*                  font;
    EFI_MEMORY_DESCRIPTOR* m_map;
    u64               m_map_size;
    u64               m_map_desc_size;
    acpi::RSDP2*           rsdp;
    u64               hhdm_offset;
    u64               kernel_phys_base;
    u64               kernel_virt_base;
};

#endif //VESPERAOS_BOOT_H