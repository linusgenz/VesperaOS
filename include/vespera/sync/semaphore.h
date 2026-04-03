// semaphore.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 03.04.26.
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
#ifndef VESPERAOS_SEMAPHORE_H
#define VESPERAOS_SEMAPHORE_H

#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

class Unit;

/**
 * @brief Counting semaphore with optional blocking wait.
 *
 * Blocking behavior requires the scheduler to be active on the calling CPU.
 * Before scheduling is up (e.g. during early boot) every wait spins instead.
 */
class Semaphore {
public:
    /**
     * @param max_count   Upper bound on the internal counter.
     * @param init_count  Starting value (must be <= max_count).
     */
    void init(u32 max_count, u32 init_count);

    /**
     * @brief Decrement the counter, blocking if it is zero.
     *
     * @param timeout_ms  0       - non-blocking try (returns false immediately).
     *                    0xFFFF  - block forever.
     *                    other   - spin/block for at most timeout_ms milliseconds.
     * @return true on success, false on timeout.
     */
    bool wait(u16 timeout_ms = 0xFFFF);

    /**
     * @brief Increment the counter by @p units, waking one blocked waiter per unit.
     */
    void signal(u32 units = 1);

private:
    struct Waiter {
        Unit*    unit{nullptr};
        Waiter*  next{nullptr};
    };

    void     push_waiter(Waiter* w);
    Waiter*  pop_waiter();           // returns nullptr if list is empty

    Spinlock lock_;
    Waiter*  head_{nullptr};
    Waiter*  tail_{nullptr};
    u32      count_{0};
    u32      max_count_{1};
};

#endif  // VESPERAOS_SEMAPHORE_H
