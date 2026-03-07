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


#include <vespera/types.h>
#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

class Channel {
    Spinlock lock_{};
    u8* buf_;  // ring buffer
    usize head_;   // write index
    usize tail_;   // read index
    int refcount_;
    explicit Channel(usize cap);
    ~Channel();

   public:
    usize used;      // wieviel bytes verfügbar sind
    usize capacity;  // totale Kapazität in bytes

    static Channel* create(usize cap);
    static void destroy(void* res);

    // return: bytes written (>=0) or negative errno
    isize send(const void* data, usize len);

    // return: bytes read (>=0) or negative errno, 0 if empty
    isize recv(void* out, usize len);
};

#endif  // VESPERAOS_CHANNEL_H