// channel.h
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

#ifndef VESPERAOS_CHANNEL_H
#define VESPERAOS_CHANNEL_H

#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

#include "klib/result.h"
#include "vespera/sync/wait_queue.h"

class Channel;

struct ChannelEndpoint {
    Channel* channel;
    bool is_reader;
    bool is_writer;
    int refcount;

    static ChannelEndpoint* create(const usize capacity, bool r, bool w);
    static ChannelEndpoint* create_with_channel(Channel* ch, bool r, bool w);
    static void ref(void* res);
    static void destroy(void* res);
};

struct PipePair {
    ChannelEndpoint* read_end;
    ChannelEndpoint* write_end;
};


class Channel {
    Spinlock lock_;
    u8* buf_;    // ring buffer
    usize head_; // write index
    usize tail_; // read index

    int reader_count_;
    int writer_count_;

    WaitQueue open_wait_;
    WaitQueue read_wait_;
    WaitQueue write_wait_;

    explicit Channel(usize cap);
    ~Channel();

public:
    usize used;     // wieviel bytes verfügbar sind
    usize capacity; // totale Kapazität in bytes

    static Channel* create(usize cap);
    void add_reader();
    void add_writer();
    void remove_reader();
    void remove_writer();
    [[nodiscard]] bool has_writers() const;
    [[nodiscard]] bool has_readers() const;
    static void destroy(void* res);
    //static void ref(void* res);

    WaitQueue& open_wait() { return open_wait_; }

    usize free_space();
    int poll(bool is_reader, bool is_writer);

    // return: bytes written (>=0) or negative errno
    isize send(const void* data, usize len, bool blocking = false);

    // return: bytes read (>=0) or negative errno, 0 if empty
    isize recv(void* out, usize len, bool blocking = false);

    static Result<::PipePair> create_pipe(usize capacity);
};

#endif  // VESPERAOS_CHANNEL_H
