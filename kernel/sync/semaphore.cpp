// semaphore.cpp
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

#include <vespera/log.h>
#include <units/unit.h>
#include <vespera/mm/memory.h>
#include <vespera/scheduling.h>
#include <vespera/sync/semaphore.h>
#include <vespera/time.h>

void Semaphore::push_waiter(Waiter* w) {
    w->next = nullptr;
    if (!tail_) {
        head_ = tail_ = w;
    } else {
        tail_->next = w;
        tail_       = w;
    }
}

Semaphore::Waiter* Semaphore::pop_waiter() {
    if (!head_) return nullptr;
    Waiter* w = head_;
    head_     = head_->next;
    if (!head_) tail_ = nullptr;
    return w;
}

void Semaphore::init(u32 max_count, u32 init_count) {
    lock_.init("semaphore");
    max_count_ = max_count;
    count_     = (init_count <= max_count) ? init_count : max_count;
    head_      = nullptr;
    tail_      = nullptr;
}

bool Semaphore::wait(u16 timeout_ms) {
    if (!kernel::scheduling::is_curent_cpu_enabled()) {
        const u64 start = kernel::time::get_uptime_ms();

        while (true) {
            {
                SpinlockGuard g(lock_);
                if (count_ > 0) {
                    --count_;
                    return true;
                }
            }
            if (timeout_ms == 0) return false;

            if (timeout_ms != 0xFFFF) {
                const u64 elapsed_ms = (kernel::time::get_uptime_ms() - start);
                if (elapsed_ms >= timeout_ms) return false;
            }

            asm volatile("pause");
        }
    }

    const u64 start_tick = (timeout_ms != 0xFFFF && timeout_ms != 0)
                               ? kernel::time::get_uptime_ms()
                               : 0;

    while (true) {
        Unit* cur = kernel::scheduling::get_current_unit();

        lock_.lock();

        if (count_ > 0) {
            --count_;
            lock_.unlock();
            return true;
        }

        if (timeout_ms == 0) {
            lock_.unlock();
            return false;
        }

        // Timeout already expired before we even try to block.
        if (timeout_ms != 0xFFFF) {
            const u64 elapsed_ms = (kernel::time::get_uptime_ms() - start_tick);
            if (elapsed_ms >= timeout_ms) {
                lock_.unlock();
                return false;
            }
        }

        Waiter w;
        w.unit = cur;
        w.next = nullptr;
        push_waiter(&w);

        cur->state = UnitState::Blocked;
        kernel::scheduling::remove_unit(cur);

        lock_.unlock();

        while (cur->state == UnitState::Blocked) {
            kernel::scheduling::yield();
        }

        return true;
    }
}

void Semaphore::signal(u32 units) {
    for (u32 i = 0; i < units; ++i) {
        Unit* to_wake = nullptr;

        {
            SpinlockGuard g(lock_);

            Waiter* w = pop_waiter();
            if (w) {
                to_wake = w->unit;
            } else {
                if (count_ < max_count_) {
                    ++count_;
                }
            }
        }

        if (to_wake) {
            to_wake->state = UnitState::Ready;
            kernel::scheduling::add_unit(to_wake);
        }
    }
}
