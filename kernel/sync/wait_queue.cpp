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
#include <vespera/sync/poll_table.h>
#include <vespera/scheduling.h>
#include <units/unit.h>

WaitQueue::WaitQueue() {
    lock_.init();
}

void WaitQueue::add_wait(Unit *u) {
    auto *entry = new WaitQueueEntry();
    entry->unit = u;
    entry->next = nullptr;

    SpinlockGuardIrq guard(lock_);

    u->state = UnitState::Blocked;
    kernel::scheduling::remove_unit(u);

    if (!head_) {
        head_ = tail_ = entry;
    } else {
        tail_->next = entry;
        tail_ = entry;
    }
}

void WaitQueue::add_poll_wait(Unit* u, PollWaiter* waiter) {
    auto* entry = new WaitQueueEntry();
    entry->unit = u;
    entry->waiter = waiter;
    entry->next = nullptr;

    SpinlockGuardIrq guard(lock_);

    if (!head_) {
        head_ = tail_ = entry;
    } else {
        tail_->next = entry;
        tail_ = entry;
    }
}

void WaitQueue::wake_all_impl() {
    WaitQueueEntry* entry = head_;
    head_ = tail_ = nullptr;

    while (entry) {
        WaitQueueEntry* next = entry->next;

        if (entry->waiter) {
            entry->waiter->wake();
        } else if (entry->unit) {
            entry->unit->state = UnitState::Ready;
            if (entry->unit->sleep_context.wakeup_ns != 0) {
                entry->unit->sleep_context.wakeup_ns = 0;
                kernel::scheduling::remove_blocked_unit(entry->unit);
            }
            kernel::scheduling::add_unit(entry->unit);
        }

        delete entry;
        entry = next;
    }
}

void WaitQueue::wake_all() {
    SpinlockGuardIrq guard(lock_);

    wake_all_impl();
}

void WaitQueue::wake_all_irq() {
    SpinlockGuardIrq irq_guard(lock_);
    wake_all_impl();
}

void WaitQueue::wake_one_impl() {
    while (head_) {
        WaitQueueEntry* entry = head_;
        head_ = head_->next;
        if (!head_) tail_ = nullptr;

        if (entry->waiter) {
            entry->waiter->wake();
            delete entry;
            continue;
        }

        if (entry->unit) {
            entry->unit->state = UnitState::Ready;
            if (entry->unit->sleep_context.wakeup_ns != 0) {
                entry->unit->sleep_context.wakeup_ns = 0;
                kernel::scheduling::remove_blocked_unit(entry->unit);
            }
            kernel::scheduling::add_unit(entry->unit);
        }

        delete entry;
        break;
    }
}

void WaitQueue::wake_one() {
    SpinlockGuardIrq guard(lock_);
    wake_one_impl();
}

void WaitQueue::wake_one_irq() {
    SpinlockGuardIrq irq_guard(lock_);
    wake_one_impl();
}

bool WaitQueue::remove(const Unit *u) {
    SpinlockGuardIrq guard(lock_);

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
    SpinlockGuardIrq guard(lock_);
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

            if (cur->waiter) {
                cur->waiter->wake();
            } else if (cur->unit) {
                cur->unit->state = UnitState::Ready;
                if (cur->unit->sleep_context.wakeup_ns != 0) {
                    cur->unit->sleep_context.wakeup_ns = 0;
                    kernel::scheduling::remove_blocked_unit(cur->unit);
                }
                kernel::scheduling::add_unit(cur->unit);
                ++woken;
            }

            delete cur;
            cur = next;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }

    return woken;
}
