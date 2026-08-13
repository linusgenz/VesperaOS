// gpu_api.h
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


#ifndef VESPLIB_GPU_API_H
#define VESPLIB_GPU_API_H

#include <stdint.h>
#include <stdbool.h>
#include <vespera/gpu.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gpu_device* gpu_device_t;
typedef struct gpu_buffer* gpu_buffer_t;   // GPU-visible memory allocation
typedef struct gpu_context* gpu_context_t; // Submission context (RCS logical state)
typedef uint64_t gpu_fence_t;              // Sequence number (seqno)

// ---------------------------------------------------------------------
// Device / Context Lifecycle
// ---------------------------------------------------------------------

gpu_device_t gpu_open(uint32_t adapter_index);
void gpu_close(gpu_device_t dev);

void gpu_query_info(gpu_device_t dev, gpu_device_info_t* out_info);

// Context corresponds to iris_screen HW context (holds logical ring state)
gpu_context_t gpu_context_create(gpu_device_t dev);
void gpu_context_destroy(gpu_context_t ctx);

// ---------------------------------------------------------------------
// Memory & Buffer Management
// ---------------------------------------------------------------------

typedef enum {
    GPU_MEM_CACHED        = 0,      // CPU cached; requires explicit flush before GPU read
    GPU_MEM_UNCACHED      = 1 << 0, // Uncached access
    GPU_MEM_WRITE_COMBINE = 1 << 1, // Optimized for CPU-write-mostly data (vertices/uniforms)
} gpu_mem_flags_t;

typedef enum {
    GPU_TILING_NONE = 0,
    GPU_TILING_X    = 1, // Legacy X-tiling
    GPU_TILING_Y    = 2, // Y-tiling (required for Iris color/depth surfaces on Gen9)
} gpu_tiling_t;

typedef struct {
    uint64_t size_bytes;
    gpu_mem_flags_t mem_flags;
    gpu_tiling_t tiling;     // Computed by ISL; passed through by winsys
    uint32_t pitch;          // 0 for linear/computed; explicit pitch for tiled surfaces
    const char* debug_label; // Optional label for debugging
} gpu_buffer_desc_t;

gpu_buffer_t gpu_buffer_create(gpu_device_t dev, const gpu_buffer_desc_t* desc);
void gpu_buffer_destroy(gpu_buffer_t buf);

// Persistent CPU mapping (map once, reuse pointer)
void* gpu_buffer_map(gpu_buffer_t buf);
void gpu_buffer_unmap(gpu_buffer_t buf);

// Returns GGTT virtual address required for STATE_BASE_ADDRESS and surface states
uint64_t gpu_buffer_gpu_addr(gpu_buffer_t buf);

// Flush CPU cache lines (required for GPU_MEM_CACHED buffers)
void gpu_buffer_flush_cpu_writes(gpu_buffer_t buf, uint64_t offset, uint64_t size);

// ---------------------------------------------------------------------
// Command Submission & Synchronization
// ---------------------------------------------------------------------

// Command batch submission without relocations (uses stable GGTT softpin addresses)
typedef struct {
    gpu_buffer_t cmd_buffer;           // Buffer containing ring commands
    uint64_t cmd_buffer_size;          // Execution size in bytes
    const gpu_buffer_t* referenced_buffers; // Referenced BOs for residency/lifetime tracking
    uint32_t referenced_buffer_count;
} gpu_submit_desc_t;

// Non-blocking asynchronous submission; returns completion fence (seqno)
gpu_fence_t gpu_submit(gpu_context_t ctx, const gpu_submit_desc_t* desc);

// Blocking wait with timeout in nanoseconds (UINT64_MAX for infinity)
bool gpu_fence_wait(gpu_device_t dev, gpu_fence_t fence, uint64_t timeout_ns);

// Non-blocking fence status query
bool gpu_fence_is_signaled(gpu_device_t dev, gpu_fence_t fence);

// ---------------------------------------------------------------------
// Presentation
// ---------------------------------------------------------------------

// Passes render target to compositor (called by eglSwapBuffers)
void gpu_present(
    gpu_device_t dev, gpu_buffer_t render_target,
    uint32_t width, uint32_t height, uint32_t pitch
);

#ifdef __cplusplus
}
#endif

#endif // VESPLIB_GPU_API_H
