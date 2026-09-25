// timerfd.h
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

#ifndef VESPERAOS_TIMERFD_H
#define VESPERAOS_TIMERFD_H

#include <vespera/sync/spinlock.h>
#include <vespera/sync/wait_queue.h>
#include <vespera/types.h>

#include "callback_timer.h"

class Unit;

class Timerfd {
    Spinlock lock_;
    u64 expirations_;
    u64 interval_ns_;
    u64 next_deadline_ns_;
    bool nonblock_;
    int refcount_;
    int clockid_;
    WaitQueue wait_;
    kernel::time::callback_timer::CallbackHandle timer_handle_;

    explicit Timerfd(int clockid, bool nonblock);
    ~Timerfd() = default;

    void arm_locked(u64 value_ns, u64 interval_ns, bool abstime);
    static void on_timer_fire(void* self);

public:
    static Timerfd* create(int clockid, bool nonblock);
    static void ref(void* res);
    static void destroy(void* res);

    int poll(bool is_reader, bool is_writer);

    isize read(void* out);

    i64 settime(int flags, u64 value_ns, u64 interval_ns, bool abstime,
                u64* old_value_ns, u64* old_interval_ns);
};

#endif