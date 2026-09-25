// callback_timer.cpp
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

#include <vespera/time/callback_timer.h>
#include <vespera/time.h>
#include <vespera/sync/spinlock.h>

#include "acpi/madt.h"

namespace kernel::time::callback_timer {

    namespace {

        constexpr u16 MAX_CALLBACK_TIMERS = 64;

        struct Slot {
            bool        in_use     = false;
            u32         generation = 1;
            u64         deadline_ns = 0;
            CallbackFn  fn         = nullptr;
            void*       arg        = nullptr;
        };

        struct CpuTimerTable {
            Spinlock lock;
            Slot     slots[MAX_CALLBACK_TIMERS];
        };

        CpuTimerTable g_tables[acpi::madt::MAX_CPU_CORES];

        i32 find_free_slot_locked(const CpuTimerTable& t) {
            for (u16 i = 0; i < MAX_CALLBACK_TIMERS; ++i) {
                if (!t.slots[i].in_use) return static_cast<i32>(i);
            }
            return -1;
        }

    }  // namespace

    void init(const u8 cpu_id) {
        auto& t = g_tables[cpu_id];
        t.lock.init("callback_timer_lock");
        for (auto& s : t.slots) {
            s.in_use = false;
            s.generation = 1;
            s.deadline_ns = 0;
            s.fn = nullptr;
            s.arg = nullptr;
        }
    }

    CallbackHandle schedule(const u8 cpu_id, const u64 deadline_ns, const CallbackFn fn, void* arg) {
        auto& t = g_tables[cpu_id];
        SpinlockGuardIrq guard(t.lock);

        const i32 idx = find_free_slot_locked(t);
        if (idx < 0) {
            return INVALID_CALLBACK_HANDLE;
        }

        Slot& s = t.slots[idx];
        s.in_use = true;
        s.deadline_ns = deadline_ns;
        s.fn = fn;
        s.arg = arg;

        return CallbackHandle{cpu_id, static_cast<u16>(idx), s.generation};
    }

    void cancel(const CallbackHandle h) {
        if (!h.valid()) return;

        auto& t = g_tables[h.cpu_id];
        SpinlockGuardIrq guard(t.lock);

        Slot& s = t.slots[h.slot];

        if (!s.in_use || s.generation != h.generation) return;

        s.in_use = false;
        s.fn = nullptr;
        s.arg = nullptr;
        ++s.generation;
    }

    u64 earliest_deadline_ns(const u8 cpu_id) {
        auto& t = g_tables[cpu_id];
        SpinlockGuardIrq guard(t.lock);

        u64 min_ns = 0;
        for (const auto& s : t.slots) {
            if (!s.in_use) continue;
            if (min_ns == 0 || s.deadline_ns < min_ns) min_ns = s.deadline_ns;
        }
        return min_ns;
    }

    void run_due_callbacks(const u8 cpu_id) {
        auto& t = g_tables[cpu_id];

        struct DueEntry {
            CallbackFn fn;
            void* arg;
        };
        DueEntry due[MAX_CALLBACK_TIMERS];
        u32 due_count = 0;

        const u64 now = kernel::time::get_uptime_ns();

        {
            SpinlockGuardIrq guard(t.lock);
            for (u16 i = 0; i < MAX_CALLBACK_TIMERS; ++i) {
                Slot& s = t.slots[i];
                if (!s.in_use) continue;
                if (s.deadline_ns > now) continue;

                due[due_count++] = DueEntry{s.fn, s.arg};

                s.in_use = false;
                s.fn = nullptr;
                s.arg = nullptr;
                ++s.generation;
            }
        }

        for (u32 i = 0; i < due_count; ++i) {
            due[i].fn(due[i].arg);
        }
    }

}  // namespace kernel::time::callback_timer