// gpu_blt_queue.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 21.05.26.
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

#ifndef VESPERAOS_GPU_BLT_QUEUE_H
#define VESPERAOS_GPU_BLT_QUEUE_H

#include <klib/intrusive_queue.h>
#include <vespera/sync/semaphore.h>
#include <vespera/sync/spinlock.h>
#include <vespera/sync/atomic.h>
#include <vespera/types.h>

enum class GpuBltOp : u8 {
    BlitRegion,
    FillRect,
};

struct GpuBltRequest {
    GpuBltRequest* next = nullptr;

    GpuBltOp op{};

    u32* owned_pixels = nullptr;
    u32 src_stride = 0;
    u32 src_x = 0, src_y = 0;

    u32 dst_x = 0, dst_y = 0;
    u32 w = 0, h = 0;

    // FillRect
    u32 color = 0;

    isize result = 0;
    AtomicFlag* done = nullptr;
};

class GpuBltQueue {
   public:
    void init();
    void shutdown();
    void submit(GpuBltRequest* req);
    GpuBltRequest* dequeue_blocking();
    [[nodiscard]] bool is_running() const {
        return running_;
    }

   private:
    Spinlock lock_;
    IntrusiveQueue<GpuBltRequest> queue_;
    Semaphore pending_;
    volatile bool running_ = false;
};

#endif  // VESPERAOS_GPU_BLT_QUEUE_H
