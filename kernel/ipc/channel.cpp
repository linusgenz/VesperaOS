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

#include <utils.h>
#include <kernel/ipc/channel.h>

#include <errno.h>
#include <log.h>
#include <kernel/memory.h>

Channel::Channel(const size_t cap) : head(0), tail(0), refcount(1), used(0), capacity(cap) {
    buf = static_cast<uint8_t *>(kernel::memory::malloc(cap));
    lock.init("channel_lock");
}

Channel *Channel::create(const size_t cap) {
    auto *ch = new Channel(cap);
    if (!ch->buf) {
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
    if (buf) kernel::memory::free(buf);
}

ssize_t Channel::send(const void *data, const size_t len) {
    if (!data || len == 0) return 0;

    spinlock_guard g(lock);
    const size_t free_space = capacity - used;
    if (free_space == 0) return -EAGAIN;

    const size_t to_write = (len < free_space) ? len : free_space;

    const size_t first = min(to_write, capacity - head);
    memcpy(buf + head, data, first);
    if (first < to_write) {
        memcpy(buf, static_cast<const uint8_t*>(data) + first, to_write - first);
    }
    head = (head + to_write) % capacity;
    used += to_write;

    return static_cast<ssize_t>(to_write);
}

ssize_t Channel::recv(void *out, const size_t len) {
    if (!out || len == 0) return 0;

    spinlock_guard g(lock);
    if (used == 0) return -EAGAIN;

    const size_t to_read = (len < used) ? len : used;

    const size_t first = min(to_read, capacity - tail);
    memcpy(out, buf + tail, first);
    if (first < to_read) {
        memcpy(static_cast<uint8_t*>(out) + first, buf, to_read - first);
    }
    tail = (tail + to_read) % capacity;
    used -= to_read;

    return static_cast<ssize_t>(to_read);
}
