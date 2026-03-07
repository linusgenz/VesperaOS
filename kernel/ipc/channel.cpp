// channel.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 01.10.25.
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

#include <vespera/ipc/channel.h>
#include <vespera_errno.h>

#include <klib/utils.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

Channel::Channel(const usize cap)
    : buf_(static_cast<u8 *>(kernel::memory::malloc(cap)))
    , head_(0)
    , tail_(0)
    , refcount_(1)
    , used(0)
    , capacity(cap) {
    lock_.init("channel_lock");
}


/**
 *
 * @param cap capacity in bytes
 * @return a channel on success, on error nullptr
 */
Channel *Channel::create(const usize cap) {
    auto *ch = new Channel(cap);
    if (!ch->buf_) {
        delete ch;
        return nullptr;
    }
    return ch;
}

void Channel::destroy(void *res) {
    const auto *c = static_cast<Channel *>(res);
    delete c;
}

Channel::~Channel() {
    if (buf_) kernel::memory::free(buf_);
}

isize Channel::send(const void *data, const usize len) {
    if (!data || len == 0) return 0;

    SpinlockGuard g(lock_);
    const usize free_space = capacity - used;
    if (free_space == 0) return -EAGAIN;

    const usize to_write = (len < free_space) ? len : free_space;

    const usize first = min(to_write, capacity - head_);
    memcpy(buf_ + head_, data, first);
    if (first < to_write) {
        memcpy(buf_, static_cast<const u8 *>(data) + first, to_write - first);
    }
    head_ = (head_ + to_write) % capacity;
    used += to_write;

    return static_cast<isize>(to_write);
}

isize Channel::recv(void *out, const usize len) {
    if (!out || len == 0) return 0;

    SpinlockGuard g(lock_);
    if (used == 0) return -EAGAIN;

    const usize to_read = (len < used) ? len : used;

    const usize first = min(to_read, capacity - tail_);
    memcpy(out, buf_ + tail_, first);
    if (first < to_read) {
        memcpy(static_cast<u8 *>(out) + first, buf_, to_read - first);
    }
    tail_ = (tail_ + to_read) % capacity;
    used -= to_read;

    return static_cast<isize>(to_read);
}
