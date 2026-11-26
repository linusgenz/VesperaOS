// wait_queue.h
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

#ifndef VESPERAOS_WAIT_QUEUE_H
#define VESPERAOS_WAIT_QUEUE_H

#include <vespera/sync/spinlock.h>

class Unit;
class PollWaiter;

struct WaitQueueEntry {
    Unit *unit{nullptr};
    PollWaiter* waiter{nullptr};
    WaitQueueEntry *next{nullptr};
};

class WaitQueue {
    Spinlock lock_;
    WaitQueueEntry *head_{nullptr};
    WaitQueueEntry *tail_{nullptr};

   public:
    WaitQueue();
    // We set wakeup_ns, make sure to set the wakeup_ns after adding into waitqueue
    void add_wait(Unit *u);
    void wake_all();
    void wake_all_irq();
    void wake_one();
    void wake_one_irq();
    bool remove(const Unit *u);
    u32 wake_matching(u32 max_wake, bool (*predicate)(const Unit*));
    void add_poll_wait(Unit* u, PollWaiter* waiter);

private:
    void wake_one_impl();
    void wake_all_impl();
};

#endif  // VESPERAOS_WAIT_QUEUE_H