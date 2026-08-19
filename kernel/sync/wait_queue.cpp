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
#include <units/unit.h>

#include "vespera/log.h"

WaitQueue::WaitQueue() {
    lock_.init();
}

void WaitQueue::add_wait(Unit *u) {
    auto *entry = new WaitQueueEntry();
    entry->unit = u;
    entry->next = nullptr;

    u->state = UnitState::Blocked;
    kernel::scheduling::remove_unit(u);

    SpinlockGuard guard(lock_);

    if (!head_) {
        head_ = tail_ = entry;
    } else {
        tail_->next = entry;
        tail_ = entry;
    }
}

void WaitQueue::wake_all() {
    SpinlockGuard guard(lock_);

    const WaitQueueEntry *entry = head_;
    while (entry) {
        if (entry->unit) {
            entry->unit->state = UnitState::Ready;

            if (entry->unit->sleep_context.wakeup_ns != 0) {
                kernel::scheduling::remove_blocked_unit(entry->unit);
            }
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
        if (entry->unit->sleep_context.wakeup_ns != 0)
            kernel::scheduling::remove_blocked_unit(entry->unit);
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

u32 WaitQueue::wake_matching(u32 max_wake, bool (*predicate)(const Unit*)) {
    SpinlockGuard guard(lock_);
    u32 woken = 0;
    WaitQueueEntry* prev = nullptr;
    WaitQueueEntry* cur = head_;

    while (cur && woken < max_wake) {
        if (predicate(cur->unit)) {
            WaitQueueEntry* next = cur->next;
            if (prev) {
                prev->next = next;
            } else {
                head_ = next;
            }
            if (cur == tail_) tail_ = prev;

            if (cur->unit) {
                cur->unit->state = UnitState::Ready;
                kernel::scheduling::remove_blocked_unit(cur->unit);
                kernel::scheduling::add_unit(cur->unit);
            }
            delete cur;
            cur = next;
            ++woken;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }

    return woken;
}





















WaitQueueDbg::WaitQueueDbg() {
    lock_.init();
}

void WaitQueueDbg::add_wait(Unit *u) {
    auto *entry = new WaitQueueEntry();
    entry->unit = u;
    entry->next = nullptr;

    Log::debug("WaitQueue::add_wait this=%p unit=%p setting Blocked + remove_unit", this, u);
    u->state = UnitState::Blocked;
    kernel::scheduling::remove_unit(u);
    Log::debug("WaitQueue::add_wait this=%p unit=%p remove_unit returned, acquiring lock_", this, u);

    SpinlockGuard guard(lock_);
    Log::debug("WaitQueue::add_wait this=%p unit=%p acquired lock_", this, u);

    if (!head_) {
        head_ = tail_ = entry;
    } else {
        tail_->next = entry;
        tail_ = entry;
    }
}

void WaitQueueDbg::wake_all() {
    Log::debug("WaitQueue::wake_all this=%p about to acquire lock_", this);
    SpinlockGuard guard(lock_);
    Log::debug("WaitQueue::wake_all this=%p acquired lock_, head_=%p", this, head_);

    const WaitQueueEntry *entry = head_;
    while (entry) {
        if (entry->unit) {
            Log::debug("WaitQueue::wake_all this=%p waking unit=%p", this, entry->unit);
            entry->unit->state = UnitState::Ready;

            if (entry->unit->sleep_context.wakeup_ns != 0) {
                Log::debug("WaitQueue::wake_all this=%p unit=%p has wakeup_ns!=0, calling remove_blocked_unit", this, entry->unit);
                kernel::scheduling::remove_blocked_unit(entry->unit);
                Log::debug("WaitQueue::wake_all this=%p unit=%p remove_blocked_unit returned", this, entry->unit);
            }
            Log::debug("WaitQueue::wake_all this=%p unit=%p about to call add_unit", this, entry->unit);
            kernel::scheduling::add_unit(entry->unit);
            Log::debug("WaitQueue::wake_all this=%p unit=%p add_unit returned", this, entry->unit);
        }

        const WaitQueueEntry *next = entry->next;
        delete entry;
        entry = next;
    }

    head_ = tail_ = nullptr;
    Log::debug("WaitQueue::wake_all this=%p done", this);
}

void WaitQueueDbg::wake_one() {
    SpinlockGuard guard(lock_);

    if (!head_) return;

    const WaitQueueEntry *entry = head_;
    head_ = head_->next;
    if (!head_) tail_ = nullptr;

    if (entry->unit) {
        entry->unit->state = UnitState::Ready;
        if (entry->unit->sleep_context.wakeup_ns != 0)
            kernel::scheduling::remove_blocked_unit(entry->unit);
        kernel::scheduling::add_unit(entry->unit);
    }

    delete entry;
}

bool WaitQueueDbg::remove(const Unit *u) {
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

u32 WaitQueueDbg::wake_matching(u32 max_wake, bool (*predicate)(const Unit*)) {
    SpinlockGuard guard(lock_);
    u32 woken = 0;
    WaitQueueEntry* prev = nullptr;
    WaitQueueEntry* cur = head_;

    while (cur && woken < max_wake) {
        if (predicate(cur->unit)) {
            WaitQueueEntry* next = cur->next;
            if (prev) {
                prev->next = next;
            } else {
                head_ = next;
            }
            if (cur == tail_) tail_ = prev;

            if (cur->unit) {
                cur->unit->state = UnitState::Ready;
                kernel::scheduling::remove_blocked_unit(cur->unit);
                kernel::scheduling::add_unit(cur->unit);
            }
            delete cur;
            cur = next;
            ++woken;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }

    return woken;
}