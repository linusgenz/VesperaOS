// eventfd.h
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

#ifndef VESPERAOS_EVENTFD_H
#define VESPERAOS_EVENTFD_H

#include <vespera/sync/spinlock.h>
#include <vespera/sync/wait_queue.h>
#include <vespera/types.h>

class Unit;

class Eventfd {
    Spinlock lock_;
    u64 counter_;
    bool semaphore_mode_;
    bool nonblock_;
    int refcount_;
    WaitQueue wait_;

    explicit Eventfd(u64 initval, bool semaphore_mode, bool nonblock);

public:
    static Eventfd* create(u64 initval, bool semaphore_mode, bool nonblock);
    static void ref(void* res);
    static void destroy(void* res);

    int poll(bool is_reader, bool is_writer);

    isize read(void* out);

    isize write(u64 val);
};

#endif // VESPERAOS_EVENTFD_H