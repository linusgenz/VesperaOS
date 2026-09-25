// timerfd.cpp
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

#include <vespera/time/timerfd.h>
#include <vespera/scheduling.h>
#include <vespera/time.h>
#include <vespera_errno.h>

#include "uapi/vespera/poll.h"

Timerfd::Timerfd(const int clockid, const bool nonblock)
    : expirations_(0), interval_ns_(0), next_deadline_ns_(0),
      nonblock_(nonblock), refcount_(1), clockid_(clockid), timer_handle_(nullptr) {
    lock_.init("timerfd_lock");
}

Timerfd* Timerfd::create(const int clockid, const bool nonblock) {
    return new Timerfd(clockid, nonblock);
}

void Timerfd::ref(void* res) {
    if (!res) return;
    __sync_add_and_fetch(&static_cast<Timerfd*>(res)->refcount_, 1);
}

void Timerfd::destroy(void* res) {
    if (!res) return;
    auto* tfd = static_cast<Timerfd*>(res);
    if (__sync_sub_and_fetch(&tfd->refcount_, 1) != 0) return;

    // Wichtig: laufenden Timer-Callback canceln, bevor delete,
    // sonst greift der Callback auf freed memory zu.
   // if (tfd->timer_handle_) kernel::time::cancel_callback(tfd->timer_handle_);
    delete tfd;
}

int Timerfd::poll(bool is_reader, bool is_writer) {
    SpinlockGuard g(lock_);
    int mask = 0;
    if (is_reader && expirations_ > 0) mask |= POLLIN;
    return mask;
}

isize Timerfd::read(void* out) {
    while (true) {
        Unit* cur = nonblock_ ? nullptr : kernel::scheduling::get_current_unit();

        lock_.lock();

        if (expirations_ == 0) {
            if (nonblock_ || !cur) {
                lock_.unlock();
                return -EAGAIN;
            }
            wait_.add_wait(cur);
            lock_.unlock();
            kernel::scheduling::yield();
            continue;
        }

        const u64 val = expirations_;
        expirations_ = 0;

        lock_.unlock();

        *static_cast<u64*>(out) = val;
        return sizeof(u64);
    }
}

// Wird vom Timer-Subsystem aufgerufen, wenn die Deadline erreicht ist.
void Timerfd::on_timer_fire(void* self_ptr) {
    auto* self = static_cast<Timerfd*>(self_ptr);

    self->lock_.lock();
    self->expirations_++;

    if (self->interval_ns_ > 0) {
        // periodisch: nächste Deadline setzen und neu einplanen
        self->next_deadline_ns_ += self->interval_ns_;
       // self->timer_handle_ = kernel::time::schedule_callback(
       //     self->next_deadline_ns_, on_timer_fire, self);
    } else {
        self->next_deadline_ns_ = 0;
        self->timer_handle_ = nullptr;
    }

    self->lock_.unlock();
    self->wait_.wake_all();
}

void Timerfd::arm_locked(const u64 value_ns, const u64 interval_ns, const bool abstime) {
    if (timer_handle_) {
      //  kernel::time::cancel_callback(timer_handle_);
        timer_handle_ = nullptr;
    }

    interval_ns_ = interval_ns;

    if (value_ns == 0) {
        // disarm
        next_deadline_ns_ = 0;
        return;
    }

    next_deadline_ns_ = abstime ? value_ns : kernel::time::get_uptime_ns() + value_ns;
   // timer_handle_ = kernel::time::schedule_callback(next_deadline_ns_, on_timer_fire, this);
}

i64 Timerfd::settime(const int flags, const u64 value_ns, const u64 interval_ns, const bool abstime,
                      u64* old_value_ns, u64* old_interval_ns) {
    SpinlockGuard g(lock_);

    if (old_value_ns) {
        // Für nicht-periodische/relative Rückgabe müsstest du hier
        // next_deadline_ns_ - now in ein relatives Delay umrechnen (siehe POSIX).
        *old_value_ns = next_deadline_ns_;
    }
    if (old_interval_ns) *old_interval_ns = interval_ns_;

    arm_locked(value_ns, interval_ns, abstime);
    return 0;
}