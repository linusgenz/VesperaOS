// poll_table.h
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

#ifndef VESPERAOS_POLL_TABLE_H
#define VESPERAOS_POLL_TABLE_H

#include <vespera/sync/spinlock.h>
#include <vespera/sync/wait_queue.h>
#include <vespera/types.h>

class Unit;
class WaitQueue;

class PollWaiter {
public:
    explicit PollWaiter(Unit* unit);

    void reset();
    void wake();

    // Puts unit_ to sleep if no wakeup has been received yet.
    // deadline_ns == U64_MAX -> wait indefinitely.
    void sleep(u64 deadline_ns);

private:
    Spinlock lock_;
    Unit* unit_;
    bool woken_{false};
    bool sleeping_{false};
};

class PollTable {
    static constexpr usize INLINE_CAP = 16;

public:
    explicit PollTable(Unit* unit);

    PollTable(const PollTable&) = delete;
    PollTable& operator=(const PollTable&) = delete;

    ~PollTable();

    void reset();
    void add(WaitQueue& wq);
    void sleep(u64 deadline_ns);
    void unregister_all();

    void set_events(i16 events) { current_events_ = events; }
    [[nodiscard]] bool wants(int mask) const { return (current_events_ & mask) != 0; }

private:
    bool grow();

    Unit* unit_;
    PollWaiter waiter_;
    WaitQueue* inline_queues_[INLINE_CAP]{};
    WaitQueue** heap_queues_{nullptr};
    WaitQueue** queues_{inline_queues_};
    usize count_{0};
    usize capacity_{INLINE_CAP};
    i16 current_events_{0};
};

#endif // VESPERAOS_POLL_TABLE_H
