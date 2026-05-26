// gpu_blt_queue.cpp
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

#include "gpu_blt_queue.h"

#include <vespera/log.h>

void GpuBltQueue::init() {
    lock_.init("gpu_blt_queue");
    pending_.init(U32_MAX, 0);
    running_ = true;
}

void GpuBltQueue::shutdown() {
    running_ = false;
    pending_.signal();
}

void GpuBltQueue::submit(GpuBltRequest* req) {
    req->next = nullptr;
    {
        SpinlockGuard g(lock_);
        queue_.push(req);
    }
    pending_.signal();
}

GpuBltRequest* GpuBltQueue::dequeue_blocking() {
    while (true) {
        pending_.wait();

        if (!running_) return nullptr;

        SpinlockGuard g(lock_);
        GpuBltRequest* req = queue_.pop();
        if (req) {
            Log::log_dbc("[blt_queue] dequeue: got req=%p op=%u", req, req->op);
            return req;
        }
        Log::log_dbc("[blt_queue] dequeue: WARN spurious wakeup, queue empty");
    }
}