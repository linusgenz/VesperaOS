// poll_table.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
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

#include <vespera/sync/poll_table.h>

#include <units/unit.h>
#include <vespera/mm/memory.h>
#include <vespera/scheduling.h>
#include <vespera/sync/wait_queue.h>
#include <vespera/time.h>

PollWaiter::PollWaiter(Unit* unit) : unit_(unit) {
    lock_.init("poll_waiter");
}

void PollWaiter::reset() {
    SpinlockGuardIrq guard(lock_);
    woken_ = false;
    sleeping_ = false;
}

void PollWaiter::wake() {
    Unit* to_wake = nullptr;
    {
        SpinlockGuardIrq guard(lock_);
        woken_ = true;
        if (sleeping_) {
            sleeping_ = false;
            to_wake = unit_;
        }
    }

    if (to_wake) {
        kernel::scheduling::add_unit(to_wake);
    }
}

void PollWaiter::sleep(const u64 deadline_ns) {
    {
        SpinlockGuardIrq guard(lock_);
        if (woken_) return;

        if (deadline_ns != U64_MAX) {
            if (kernel::time::get_uptime_ns() >= deadline_ns) return;

            sleeping_ = true;
            unit_->sleep_context.wakeup_ns = deadline_ns;
            kernel::scheduling::add_blocked_unit(unit_, unit_->cpu_id);
            kernel::time::sleep_timer::notify_sleep(unit_->cpu_id, deadline_ns);
        } else {
            sleeping_ = true;
            unit_->sleep_context.wakeup_ns = 0;
            unit_->state = UnitState::Blocked;
            kernel::scheduling::remove_unit(unit_);
        }
    }

    kernel::scheduling::yield();

    {
        SpinlockGuardIrq guard(lock_);
        sleeping_ = false;
    }

    if (deadline_ns != U64_MAX) {
        unit_->sleep_context.wakeup_ns = 0;
        kernel::scheduling::remove_blocked_unit(unit_);
    }
}

// --- PollTable ---

PollTable::PollTable(Unit* unit) : unit_(unit), waiter_(unit) {}

PollTable::~PollTable() {
    unregister_all();
    if (heap_queues_) {
        kernel::memory::free(heap_queues_);
    }
}

void PollTable::reset() {
    unregister_all();
    waiter_.reset();
}

void PollTable::add(WaitQueue& wq) {
    for (usize i = 0; i < count_; ++i) {
        if (queues_[i] == &wq) return;
    }

    if (count_ == capacity_) {
        if (!grow()) return;
    }

    queues_[count_++] = &wq;
    wq.add_poll_wait(unit_, &waiter_);
}

void PollTable::sleep(const u64 deadline_ns) {
    waiter_.sleep(deadline_ns);
}

void PollTable::unregister_all() {
    for (usize i = 0; i < count_; ++i) {
        queues_[i]->remove(unit_);
    }
    count_ = 0;
}

bool PollTable::grow() {
    const usize new_cap = capacity_ * 2;
    auto** next = static_cast<WaitQueue**>(
        kernel::memory::malloc(new_cap * sizeof(WaitQueue*))
    );
    if (!next) return false;

    for (usize i = 0; i < count_; ++i) next[i] = queues_[i];
    if (heap_queues_) kernel::memory::free(heap_queues_);

    heap_queues_ = next;
    queues_ = next;
    capacity_ = new_cap;
    return true;
}