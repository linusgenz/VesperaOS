// wait_queue.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 23.11.25.
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

#include <vespera/sync/wait_queue.h>

#include <vespera/scheduling.h>

#include "../units/unit.h"

WaitQueue::WaitQueue() {
    lock_.init();
}

void WaitQueue::add_wait(Unit *u) {
    auto *entry = new WaitQueueEntry();
    entry->unit = u;
    entry->next = nullptr;

    {
        SpinlockGuard guard(lock_);

        if (!head_) {
            head_ = tail_ = entry;
        } else {
            tail_->next = entry;
            tail_ = entry;
        }
    }

    u->state = UnitState::Blocked;

    kernel::scheduling::remove_unit(u);
}

void WaitQueue::wake_all() {
    SpinlockGuard guard(lock_);

    const WaitQueueEntry *entry = head_;
    while (entry) {
        if (entry->unit) {
            entry->unit->state = UnitState::Ready;

            kernel::scheduling::add_unit(entry->unit);
        }

        const WaitQueueEntry *next = entry->next;
        delete entry;
        entry = next;
    }

    head_ = tail_ = nullptr;
}

void WaitQueue::wake_one() {
    SpinlockGuard guard(lock_);

    if (!head_) return;

    const WaitQueueEntry *entry = head_;
    head_ = head_->next;
    if (!head_) tail_ = nullptr;

    if (entry->unit) {
        entry->unit->state = UnitState::Ready;
        kernel::scheduling::add_unit(entry->unit);
    }

    delete entry;
}

bool WaitQueue::remove(const Unit *u) {
    SpinlockGuard guard(lock_);

    if (!head_) return false;

    if (head_->unit == u) {
        const WaitQueueEntry *tmp = head_;
        head_ = head_->next;
        if (!head_) tail_ = nullptr;
        delete tmp;
        return true;
    }

    WaitQueueEntry *prev = head_;
    WaitQueueEntry *cur = head_->next;

    while (cur) {
        if (cur->unit == u) {
            prev->next = cur->next;
            if (cur == tail_) tail_ = prev;
            delete cur;
            return true;
        }
        prev = cur;
        cur = cur->next;
    }

    return false;
}