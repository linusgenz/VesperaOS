// signalfd.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 26.09.26.
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

#ifndef VESPERAOS_SIGNALFD_H
#define VESPERAOS_SIGNALFD_H

#include <vespera/sync/spinlock.h>
#include <vespera/sync/wait_queue.h>
#include <vespera/types.h>

class Realm;
enum class Signal : u32;

class Signalfd {
    Spinlock lock_;
    Realm* owner_;
    u64 mask_;
    bool nonblock_;
    int refcount_;
    WaitQueue wait_;

    explicit Signalfd(Realm* owner, u64 mask, bool nonblock);
    ~Signalfd() = default;

public:
    static Signalfd* create(Realm* owner, u64 mask, bool nonblock);
    static void ref(void* res);
    static void destroy(void* res);

    int poll(bool is_reader, bool is_writer);

    isize read(void* out, usize count);

    void set_mask(u64 mask);

    void notify(Signal sig);

    Signalfd* next; // intrusive, Realm::signalfd_list
};

#endif  // VESPERAOS_SIGNALFD_H
