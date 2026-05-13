// block_io_queue.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 11.04.26.
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
#ifndef VESPERAOS_BLOCK_IO_QUEUE_H
#define VESPERAOS_BLOCK_IO_QUEUE_H

#include <klib/intrusive_queue.h>
#include <vespera/sync/semaphore.h>
#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

enum class BlockIoOp : u8 {
    Read,
    Write,
    Flush,
};

struct BlockIoRequest {
    BlockIoRequest* next = nullptr;

    BlockIoOp op{};
    u64 lba{};
    usize sector_count{};
    void* buffer{};
    usize buffer_size{};

    isize result = 0;

    Semaphore done;
};

class BlockIoQueue {
   public:
    void init();
    void shutdown();

    void submit(BlockIoRequest* req);

    BlockIoRequest* dequeue_blocking();

    [[nodiscard]] bool is_running() const {
        return running_;
    }

   private:
    Spinlock lock_;
    IntrusiveQueue<BlockIoRequest> queue_;
    Semaphore pending_;
    volatile bool running_ = false;
};

#endif  // VESPERAOS_BLOCK_IO_QUEUE_H
