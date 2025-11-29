// xhci_mem.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 21.07.25.
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
 
#ifndef XHCI_MEM_H
#define XHCI_MEM_H
#include <kernel/memory.h>

// Memory Alignment and Boundary Definitions
#define XHCI_DEVICE_CONTEXT_INDEX_MAX_SIZE      2048
#define XHCI_DEVICE_CONTEXT_MAX_SIZE            2048
#define XHCI_INPUT_CONTROL_CONTEXT_MAX_SIZE     64
#define XHCI_SLOT_CONTEXT_MAX_SIZE              64
#define XHCI_ENDPOINT_CONTEXT_MAX_SIZE          64
#define XHCI_STREAM_CONTEXT_MAX_SIZE            16
#define XHCI_STREAM_ARRAY_LINEAR_MAX_SIZE       1024 * 1024 // 1 MB
#define XHCI_STREAM_ARRAY_PRI_SEC_MAX_SIZE      PAGE_SIZE
#define XHCI_TRANSFER_RING_SEGMENTS_MAX_SIZE    1024 * 64   // 64 KB
#define XHCI_COMMAND_RING_SEGMENTS_MAX_SIZE     1024 * 64   // 64 KB
#define XHCI_EVENT_RING_SEGMENTS_MAX_SIZE       1024 * 64   // 64 KB
#define XHCI_EVENT_RING_SEGMENT_TABLE_MAX_SIZE  1024 * 512  // 512 KB
#define XHCI_SCRATCHPAD_BUFFER_ARRAY_MAX_SIZE   248
#define XHCI_SCRATCHPAD_BUFFERS_MAX_SIZE        PAGE_SIZE

#define XHCI_DEVICE_CONTEXT_INDEX_BOUNDARY      PAGE_SIZE
#define XHCI_DEVICE_CONTEXT_BOUNDARY            PAGE_SIZE
#define XHCI_INPUT_CONTROL_CONTEXT_BOUNDARY     PAGE_SIZE
#define XHCI_SLOT_CONTEXT_BOUNDARY              PAGE_SIZE
#define XHCI_ENDPOINT_CONTEXT_BOUNDARY          PAGE_SIZE
#define XHCI_STREAM_CONTEXT_BOUNDARY            PAGE_SIZE
#define XHCI_STREAM_ARRAY_LINEAR_BOUNDARY       PAGE_SIZE
#define XHCI_STREAM_ARRAY_PRI_SEC_BOUNDARY      PAGE_SIZE
#define XHCI_TRANSFER_RING_SEGMENTS_BOUNDARY    1024 * 64   // 64 KB
#define XHCI_COMMAND_RING_SEGMENTS_BOUNDARY     1024 * 64   // 64 KB
#define XHCI_EVENT_RING_SEGMENTS_BOUNDARY       1024 * 64   // 64 KB
#define XHCI_EVENT_RING_SEGMENT_TABLE_BOUNDARY  PAGE_SIZE
#define XHCI_SCRATCHPAD_BUFFER_ARRAY_BOUNDARY   PAGE_SIZE
#define XHCI_SCRATCHPAD_BUFFERS_BOUNDARY        PAGE_SIZE

#define XHCI_DEVICE_CONTEXT_INDEX_ALIGNMENT      64
#define XHCI_DEVICE_CONTEXT_ALIGNMENT            64
#define XHCI_INPUT_CONTROL_CONTEXT_ALIGNMENT     64
#define XHCI_SLOT_CONTEXT_ALIGNMENT              32
#define XHCI_ENDPOINT_CONTEXT_ALIGNMENT          32
#define XHCI_STREAM_CONTEXT_ALIGNMENT            16
#define XHCI_STREAM_ARRAY_LINEAR_ALIGNMENT       16
#define XHCI_STREAM_ARRAY_PRI_SEC_ALIGNMENT      16
#define XHCI_TRANSFER_RING_SEGMENTS_ALIGNMENT    64
#define XHCI_COMMAND_RING_SEGMENTS_ALIGNMENT     64
#define XHCI_EVENT_RING_SEGMENTS_ALIGNMENT       64
#define XHCI_EVENT_RING_SEGMENT_TABLE_ALIGNMENT  64
#define XHCI_SCRATCHPAD_BUFFER_ARRAY_ALIGNMENT   64
#define XHCI_SCRATCHPAD_BUFFERS_ALIGNMENT        PAGE_SIZE

uintptr_t xhci_map_mmio(uint64_t pci_bar_address, uint32_t bar_size);

void* alloc_xhci_memory(
    size_t size,
    size_t alignment = 64,
    size_t boundary = PAGE_SIZE
);

void free_xhci_memory(void* ptr);

uintptr_t xhci_get_physical_addr(void* virt);

#endif //XHCI_MEM_H
