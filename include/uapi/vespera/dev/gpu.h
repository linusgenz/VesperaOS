// gpu.h
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
#ifndef VESPERAOS_UAPI_GPU_H
#define VESPERAOS_UAPI_GPU_H

#include <vespera/types.h>

typedef struct {
    u16 pci_device_id;     // Selects Gen9 SKU table in ISL/Iris
    u8 gen_major;          // 9
    u8 gen_minor;          // 0

    u32 eu_count;
    u32 subslice_count;
    u32 slice_count;
    u64 gtt_size_bytes;    // Usable GGTT aperture size

    u32 min_buffer_align;  // Minimum allocation alignment (e.g., PAGE_SIZE)
    u32 timestamp_freq_hz; // Frequency for GPU timestamp to ns conversion
} gpu_device_info_t;

#endif //VESPERAOS_UAPI_GPU_H
