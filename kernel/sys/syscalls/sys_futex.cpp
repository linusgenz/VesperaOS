// sys_futex.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 23.06.26.
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

#include <uapi/vespera/futex.h>
#include <uapi/vespera/time.h>
#include <vespera/types.h>
#include <vespera/scheduling.h>
#include <vespera/unit/unit_manager.h>
#include <time/clock_manager.h>
#include <vespera/time.h>

#include "vespera_errno.h"
#include "units/unit.h"
#include "vespera/log.h"

namespace syscalls::internal {

    static constexpr usize FUTEX_BUCKETS = 64;

    struct FutexBucket {
        Spinlock lock;
        WaitQueue queue;
    };

    static FutexBucket futex_table[FUTEX_BUCKETS];
    static bool futex_initialized = false;

    static void ensure_init() {
        if (futex_initialized) return;
        for (auto& b : futex_table) {
            b.lock.init("futex_bucket");
        }
        futex_initialized = true;
    }

    static FutexBucket& get_bucket(const u32* uaddr) {
        const uptr h = reinterpret_cast<uptr>(uaddr);
        return futex_table[(h >> 2) % FUTEX_BUCKETS];
    }

    // Hilfspointer für wake_matching — sicher weil immer unter bucket.lock gesetzt
    static uptr g_wake_uaddr = 0;

    static bool match_futex_uaddr(const Unit* u) {
        return u->futex_uaddr == g_wake_uaddr;
    }

    i64 sys_futex(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
        ensure_init();

        auto* uaddr        = reinterpret_cast<u32*>(arg0);
        const int op       = static_cast<int>(arg1) & 0xFF;
        const u32 val      = static_cast<u32>(arg2);
        const bool abstime = static_cast<int>(arg1) & FUTEX_ABSTIME;

        if (!uaddr || (reinterpret_cast<uptr>(uaddr) & 3))
            return -EINVAL;

        switch (op) {

        case FUTEX_WAIT: {
            FutexBucket& bucket = get_bucket(uaddr);

            bucket.lock.lock();

            const u32 current = __atomic_load_n(uaddr, __ATOMIC_SEQ_CST);
            if (current != val) {
                bucket.lock.unlock();
                return -EAGAIN;
            }

            Unit* self = kernel::scheduling::get_current_unit();
            self->futex_uaddr = reinterpret_cast<uptr>(uaddr);

            if (arg3 != 0) {
                const auto* ts = reinterpret_cast<const timespec_t*>(arg3);
                u64 timeout_ns;
                if (abstime) {
                    timeout_ns = static_cast<u64>(ts->tv_sec) * 1'000'000'000ULL + ts->tv_nsec;
                } else {
                    const u64 now = kernel::time::get_uptime_ns();
                    timeout_ns = now + static_cast<u64>(ts->tv_sec) * 1'000'000'000ULL + ts->tv_nsec;
                }
                self->sleep_context.wakeup_ns = timeout_ns;
            } else {
                self->sleep_context.wakeup_ns = 0;
            }

            bucket.queue.add_wait(self);
            bucket.lock.unlock();

            kernel::scheduling::yield();

            const bool timed_out = (arg3 != 0) &&
                (kernel::time::get_uptime_ns() >= self->sleep_context.wakeup_ns);

            self->futex_uaddr = 0;

            if (timed_out)
                return -ETIMEDOUT;

            return 0;
        }

        case FUTEX_WAKE: {
            FutexBucket& bucket = get_bucket(uaddr);
            SpinlockGuard g(bucket.lock);

            g_wake_uaddr = reinterpret_cast<uptr>(uaddr);
            const i64 woken = static_cast<i64>(bucket.queue.wake_matching(val, match_futex_uaddr));
            return woken;
        }

        case FUTEX_WAKE_ALL: {
            FutexBucket& bucket = get_bucket(uaddr);
            SpinlockGuard g(bucket.lock);

            g_wake_uaddr = reinterpret_cast<uptr>(uaddr);
            const i64 woken = bucket.queue.wake_matching(U32_MAX, match_futex_uaddr);
            return woken;
        }

        default:
            return -ENOSYS;
        }
    }

} // namespace syscalls::internal