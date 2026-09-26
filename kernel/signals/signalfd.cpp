// signalfd.cpp
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

#include <vespera/signal/signalfd.h>
#include <vespera/scheduling.h>
#include <vespera/signals.h>
#include <units/unit.h>
#include <vespera_errno.h>

#include "uapi/vespera/poll.h"
#include "uapi/vespera/signalfd.h"

#include "realm/realm.h"

Signalfd::Signalfd(Realm* owner, const u64 mask, const bool nonblock)
    : owner_(owner), mask_(mask), nonblock_(nonblock), refcount_(1), next(nullptr) {
    lock_.init("signalfd_lock");
}

Signalfd* Signalfd::create(Realm* owner, const u64 mask, const bool nonblock) {
    return new Signalfd(owner, mask, nonblock);
}

void Signalfd::ref(void* res) {
    if (!res) return;
    __sync_add_and_fetch(&static_cast<Signalfd*>(res)->refcount_, 1);
}

void Signalfd::destroy(void* res) {
    if (!res) return;
    auto* sfd = static_cast<Signalfd*>(res);
    if (__sync_sub_and_fetch(&sfd->refcount_, 1) != 0) return;

    sfd->owner_->signalfd_list.remove(sfd);
    delete sfd;
}

int Signalfd::poll(bool is_reader, bool is_writer) {
    SpinlockGuard g(lock_);
    int mask = 0;
    if (is_reader && (owner_->signals_pending & mask_)) mask |= POLLIN;
    return mask;
}

isize Signalfd::read(void* out, const usize count) {
    if (count < sizeof(signalfd_siginfo)) return -EINVAL;

    while (true) {
        Unit* cur = nonblock_ ? nullptr : kernel::scheduling::get_current_unit();

        lock_.lock();

        const u64 ready = owner_->signals_pending & mask_;

        if (!ready) {
            if (nonblock_ || !cur) {
                lock_.unlock();
                return -EAGAIN;
            }
            wait_.add_wait(cur);
            lock_.unlock();
            kernel::scheduling::yield();
            continue;
        }

        const u32 signum = __builtin_ctzll(ready);

        __sync_and_and_fetch(&owner_->signals_pending, ~(1ULL << signum));

        lock_.unlock();

        signalfd_siginfo info{};
        info.ssi_signo = signum;
        info.ssi_errno = 0;
        info.ssi_code  = 0;
        info.ssi_pid   = 0;
        info.ssi_uid   = 0;

        *static_cast<signalfd_siginfo*>(out) = info;
        return sizeof(signalfd_siginfo);
    }
}

void Signalfd::set_mask(const u64 mask) {
    SpinlockGuard g(lock_);
    mask_ = mask;
}

void Signalfd::notify(Signal sig) {
    const auto signum = static_cast<u32>(sig);

    lock_.lock();
    const bool relevant = mask_ & (1ULL << signum);
    lock_.unlock();

    if (relevant) wait_.wake_all();
}