//
// Created by linus on 02.07.25.
//

#include <vespera/interrupts.h>
#include <vespera/scheduling.h>
#include <kernel/units/unit.h>
#include <vespera/time.h>

#include "../cpu/cpu_manager.h"
#include "clock_manager.h"

namespace kernel::time {

    static u64 g_rtc_epoch_ns = 0;

    void init_clock() {
        clock_manager::init();
    }

    u64 get_uptime_ns() {
        return clock_manager::read_ns();
    }

    u64 get_uptime_us() {
        return clock_manager::read_us();
    }

    u64 get_uptime_ms() {
        return clock_manager::read_ms();
    }

    u64 get_ticks() {
        return clock_manager::read_ticks();
    }

    const char* clock_source_name() {
        return clock_manager::active_source_name();
    }

    void init_wall_clock(const u64 unix_epoch_ns) {
        g_rtc_epoch_ns = unix_epoch_ns;
    }

    u64 get_realtime_ns() {
        return get_uptime_ns() + g_rtc_epoch_ns;
    }

    namespace internal {
        bool thread_sleep_ns(const u64 ns) {
            const u32 cpu_id = cpu_manager::get_current_cpu_id();

            Unit* current = kernel::scheduling::get_current_unit();
            if (!current || current->is_idle) return true;

            current->sleep_context.wakeup_ns = get_uptime_ns() + ns;
            current->sleep_context.interrupted  = false;

            kernel::scheduling::add_blocked_unit(current, cpu_id);
            kernel::scheduling::yield();

            return !current->sleep_context.interrupted;
        }

        void busy_sleep_ns(const u64 ns) {
            const u64 deadline_ns = get_uptime_ns() + ns;
            while (get_uptime_ns() < deadline_ns) {
                asm volatile("hlt");
            }
        }
    }  // namespace internal

    void sleep_ms(const u64 ms) {
        uint64_t ns = ms * 1'000'000ULL;
        if (scheduling::is_curent_cpu_enabled()) {
            internal::thread_sleep_ns(ns);
        } else {
            internal::busy_sleep_ns(ns);
        }
    }

    void sleep_us(const u64 us) {
        uint64_t ns = us * 1'000ULL;
        if (scheduling::is_curent_cpu_enabled()) {
            internal::thread_sleep_ns(ns);
        } else {
            internal::busy_sleep_ns(ns);
        }
    }

    bool sleep_ns(const u64 ns) {
        if (scheduling::is_curent_cpu_enabled()) {
            return internal::thread_sleep_ns(ns);
        } else {
            internal::busy_sleep_ns(ns);
            return true;
        }
    }
}  // namespace kernel::time
