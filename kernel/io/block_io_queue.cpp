// block_io_queue.cpp
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

#include "vespera/io/block_io_queue.h"

void BlockIoQueue::init() {
    lock_.init();
    pending_.init(U32_MAX, 0);
    running_ = true;
}

void BlockIoQueue::shutdown() {
    running_ = false;
    pending_.signal();  // unblock dequeue_blocking() so the worker can exit
}

void BlockIoQueue::submit(BlockIoRequest* req) {
    req->next = nullptr;
    {
        SpinlockGuard g(lock_);
        queue_.push(req);
    }
    pending_.signal();
}

BlockIoRequest* BlockIoQueue::dequeue_blocking() {
    while (true) {
        pending_.wait();

        if (!running_) return nullptr;

        SpinlockGuard g(lock_);
        BlockIoRequest* req = queue_.pop();
        if (req) return req;
    }
}