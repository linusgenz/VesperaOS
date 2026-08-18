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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS.  If not, see <https://www.gnu.org/licenses/>.

#include <klib/string.h>
#include <klib/utils.h>
#include <vespera/ipc/channel.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/scheduling.h>
#include <vespera_errno.h>

#include "klib/result.h"
#include "uapi/vespera/poll.h"

Channel::Channel(const usize cap)
    : buf_(static_cast<u8*>(kernel::memory::malloc(cap)))
      , head_(0)
      , tail_(0)
      , used(0)
      , capacity(cap)
      , reader_count_(0)
      , writer_count_(0) {
    lock_.init("channel_lock");
}

Channel::~Channel() {
    if (buf_) kernel::memory::free(buf_);
}

Channel* Channel::create(const usize cap) {
    auto* ch = new Channel(cap);
    if (!ch || !ch->buf_) {
        delete ch;
        return nullptr;
    }
    return ch;
}

void Channel::destroy(void* res) {
    if (!res) return;
    const Channel* ch = static_cast<Channel*>(res);
    delete ch;
}

void Channel::add_reader() {
    __sync_add_and_fetch(&reader_count_, 1);
    // A writer may be blocked in open() waiting for the first reader.
    open_wait_.wake_all();
}

void Channel::add_writer() {
    __sync_add_and_fetch(&writer_count_, 1);
    // A reader may be blocked in open() waiting for the first writer.
    open_wait_.wake_all();
}

void Channel::remove_reader() {
    const int r = __sync_sub_and_fetch(&reader_count_, 1);
    const int w = __atomic_load_n(&writer_count_, __ATOMIC_ACQUIRE);

    if (r == 0) {
        // Last reader gone: any writer blocked in send() (buffer full) or in
        // open() (waiting for a reader to show up) must wake up and observe
        // has_readers() == false, i.e. get EPIPE.
        write_wait_.wake_all();
        open_wait_.wake_all();
    }

    if (r == 0 && w == 0) {
        delete this;
    }
}

void Channel::remove_writer() {
    const int w = __sync_sub_and_fetch(&writer_count_, 1);
    const int r = __atomic_load_n(&reader_count_, __ATOMIC_ACQUIRE);

    if (w == 0) {
        // Last writer gone: any reader blocked in recv() (buffer empty) must
        // wake up and observe has_writers() == false, i.e. get EOF (0). Any
        // opener still waiting for a writer must also be woken so it can
        // re-check (it stays blocked if it wanted a writer and none showed).
        read_wait_.wake_all();
        open_wait_.wake_all();
    }

    if (w == 0 && r == 0) {
        delete this;
    }
}

bool Channel::has_writers() const {
    return __atomic_load_n(&writer_count_, __ATOMIC_ACQUIRE) > 0;
}

bool Channel::has_readers() const {
    return __atomic_load_n(&reader_count_, __ATOMIC_ACQUIRE) > 0;
}

usize Channel::free_space() {
    SpinlockGuard g(lock_);
    return capacity - used;
}

int Channel::poll(bool is_reader, bool is_writer) {
    SpinlockGuard g(lock_);

    int mask = 0;
    const bool writers_exist = has_writers();
    const bool readers_exist = has_readers();

    if (is_reader) {
        if (used > 0 || !writers_exist) mask |= POLLIN;
        if (!writers_exist) mask |= POLLHUP;
    }

    if (is_writer) {
        if (used < capacity && readers_exist) mask |= POLLOUT;
        if (!readers_exist) mask |= POLLERR;
    }

    return mask;
}

isize Channel::send(const void* data, const usize len, const bool blocking) {
    if (!data || len == 0) return 0;

    while (true) {
        Unit* cur = blocking ? kernel::scheduling::get_current_unit() : nullptr;

        lock_.lock();

        if (!has_readers()) {
            lock_.unlock();
            return -EPIPE;
        }

        const usize space = capacity - used;
        if (space == 0) {
            if (!blocking || !cur) {
                lock_.unlock();
                return -EAGAIN;
            }

            // Block until a reader drains the buffer, a reader disappears
            // (checked again on wake -> EPIPE above), or a signal arrives.
            write_wait_.add_wait(cur);
            lock_.unlock();
            kernel::scheduling::yield();
            continue; // re-check state after waking
        }

        const usize to_write = (len < space) ? len : space;

        const usize first = min(to_write, capacity - head_);
        memcpy(buf_ + head_, data, first);
        if (first < to_write)
            memcpy(buf_, static_cast<const u8*>(data) + first, to_write - first);

        head_ = (head_ + to_write) % capacity;
        used += to_write;

        lock_.unlock();

        // Data became available: wake a blocked reader, if any.
        read_wait_.wake_one();

        return static_cast<isize>(to_write);
    }
}

isize Channel::recv(void* out, const usize len, const bool blocking) {
    if (!out || len == 0) return 0;

    while (true) {
        Unit* cur = blocking ? kernel::scheduling::get_current_unit() : nullptr;

        lock_.lock();

        if (used == 0) {
            if (!has_writers()) {
                lock_.unlock();
                return 0; // EOF
            }

            if (!blocking || !cur) {
                lock_.unlock();
                return -EAGAIN;
            }

            // Block until a writer fills the buffer, the last writer
            // disappears (checked again on wake -> EOF above), or a signal
            // arrives.
            read_wait_.add_wait(cur);
            lock_.unlock();
            kernel::scheduling::yield();
            continue; // re-check state after waking
        }

        const usize to_read = (len < used) ? len : used;

        const usize first = min(to_read, capacity - tail_);
        memcpy(out, buf_ + tail_, first);
        if (first < to_read)
            memcpy(static_cast<u8*>(out) + first, buf_, to_read - first);

        tail_ = (tail_ + to_read) % capacity;
        used -= to_read;

        lock_.unlock();

        // Space freed up: wake a blocked writer, if any.
        write_wait_.wake_one();

        return static_cast<isize>(to_read);
    }
}

// --- AB HIER: Die saubere Endpoint-Einheit ---

ChannelEndpoint* ChannelEndpoint::create_with_channel(Channel* ch, bool r, bool w) {
    if (!ch) return nullptr;
    auto* ep = new ChannelEndpoint{ch, r, w, 1};
    if (!ep) return nullptr;

    if (r) ch->add_reader();
    if (w) ch->add_writer();
    return ep;
}

ChannelEndpoint* ChannelEndpoint::create(const usize capacity, bool r, bool w) {
    Channel* ch = Channel::create(capacity);
    if (!ch) return nullptr;

    auto* ep = create_with_channel(ch, r, w);
    if (!ep) {
        Channel::destroy(ch);
        return nullptr;
    }
    return ep;
}

Result<PipePair> Channel::create_pipe(const usize capacity) {
    Channel* ch = Channel::create(capacity);
    if (!ch) return Result<PipePair>::err(Error::NoMem);

    ChannelEndpoint* read_end = ChannelEndpoint::create_with_channel(ch, /*r=*/true, /*w=*/false);
    if (!read_end) {
        delete ch;
        return Result<PipePair>::err(Error::NoMem);
    }

    ChannelEndpoint* write_end = ChannelEndpoint::create_with_channel(ch, /*r=*/false, /*w=*/true);
    if (!write_end) {
        ChannelEndpoint::destroy(read_end);
        return Result<PipePair>::err(Error::NoMem);
    }

    return Result<PipePair>::ok(PipePair{read_end, write_end});
}

void ChannelEndpoint::ref(void* res) {
    if (!res) return;
    auto* ep = static_cast<ChannelEndpoint*>(res);
    __sync_add_and_fetch(&ep->refcount, 1);
}

void ChannelEndpoint::destroy(void* res) {
    if (!res) return;
    auto* ep = static_cast<ChannelEndpoint*>(res);

    if (__sync_sub_and_fetch(&ep->refcount, 1) != 0)
        return;

    Channel* ch = ep->channel;
    const bool r = ep->is_reader;
    const bool w = ep->is_writer;

    delete ep;

    if (r) ch->remove_reader();
    if (w) ch->remove_writer();
}
