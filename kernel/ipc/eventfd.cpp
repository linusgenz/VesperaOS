// eventfd.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 25.09.26.
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

#include <vespera/ipc/eventfd.h>
#include <vespera/scheduling.h>
#include <vespera_errno.h>

#include "uapi/vespera/poll.h"


Eventfd::Eventfd(const u64 initval, const bool semaphore_mode, const bool nonblock)
    : counter_(initval), semaphore_mode_(semaphore_mode), nonblock_(nonblock), refcount_(1) {
    lock_.init("eventfd_lock");
}

Eventfd* Eventfd::create(const u64 initval, const bool semaphore_mode, const bool nonblock) {
    return new Eventfd(initval, semaphore_mode, nonblock);
}

void Eventfd::ref(void* res) {
    if (!res) return;
    auto* efd = static_cast<Eventfd*>(res);
    __sync_add_and_fetch(&efd->refcount_, 1);
}

void Eventfd::destroy(void* res) {
    if (!res) return;
    auto* efd = static_cast<Eventfd*>(res);

    if (__sync_sub_and_fetch(&efd->refcount_, 1) != 0)
        return;

    delete efd;
}

int Eventfd::poll(bool is_reader, bool is_writer) {
    SpinlockGuard g(lock_);

    int mask = 0;
    if (is_reader && counter_ > 0) mask |= POLLIN;
    if (is_writer && counter_ < U64_MAX - 1) mask |= POLLOUT;
    return mask;
}

isize Eventfd::read(void* out, const usize count) {
    if (count < sizeof(u64)) return -EINVAL;

    while (true) {
        Unit* cur = nonblock_ ? nullptr : kernel::scheduling::get_current_unit();

        lock_.lock();

        if (counter_ == 0) {
            if (nonblock_ || !cur) {
                lock_.unlock();
                return -EAGAIN;
            }
            wait_.add_wait(cur);
            lock_.unlock();
            kernel::scheduling::yield();
            continue;
        }

        const u64 val = semaphore_mode_ ? 1 : counter_;
        counter_ -= val;

        lock_.unlock();

        *static_cast<u64*>(out) = val;
        wait_.wake_all();
        return sizeof(u64);
    }
}

isize Eventfd::write(const void* in, const usize count) {
    if (count < sizeof(u64)) return -EINVAL;

    const u64 val = *static_cast<const u64*>(in);
    if (val == U64_MAX) return -EINVAL;

    while (true) {
        Unit* cur = nonblock_ ? nullptr : kernel::scheduling::get_current_unit();

        lock_.lock();

        if (counter_ >= U64_MAX - val) {
            if (nonblock_ || !cur) {
                lock_.unlock();
                return -EAGAIN;
            }
            wait_.add_wait(cur);
            lock_.unlock();
            kernel::scheduling::yield();
            continue;
        }

        counter_ += val;

        lock_.unlock();

        wait_.wake_all();
        return sizeof(u64);
    }
}