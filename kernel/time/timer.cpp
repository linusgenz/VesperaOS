//
// Created by linus on 02.07.25.
//

#include <vespera/interrupts.h>
#include <vespera/scheduling.h>
#include <vespera/time.h>

#include "../cpu/cpu_manager.h"
#include "clock_manager.h"

namespace kernel::time {

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

    namespace internal {
        void thread_sleep_ms(const u64 ms) {
            const u32 cpu_id = cpu_manager::get_current_cpu_id();

            Unit* current = kernel::scheduling::get_current_unit();
            if (!current || current->is_idle) return;

            current->sleep_context.wakeup_tick = interrupts::lapic_get_ticks(cpu_id) + (ms + 9) / 10;

            kernel::scheduling::add_blocked_unit(current, cpu_id);
            kernel::scheduling::yield();
        }

        void busy_sleep_ms(const u64 ms) {
            const u64 deadline_ns = get_uptime_ns() + ms * 1'000'000ULL;
            while (get_uptime_ns() < deadline_ns) {
                asm volatile("hlt");
            }
        }
    }  // namespace internal

    void sleep_ms(const u64 ms) {
        if (scheduling::is_curent_cpu_enabled()) {
            internal::thread_sleep_ms(ms);
        } else {
            internal::busy_sleep_ms(ms);
        }
    }
}  // namespace kernel::time
