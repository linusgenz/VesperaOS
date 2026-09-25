//
// Created by linus on 02.07.25.
//

#include <vespera/scheduling.h>
#include <units/unit.h>
#include <vespera/time.h>

#include "../cpu/cpu_manager.h"
#include "clock_manager.h"
#include "vespera/time/callback_timer.h"

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
        bool thread_sleep_until_ns(const u64 target_ns) {
            const u64 now = get_uptime_ns();
            if (target_ns <= now) {
                return true;
            }

            const u32 cpu_id = cpu_manager::get_current_cpu_id();

            Unit* current = kernel::scheduling::get_current_unit();
            if (!current || current->is_idle) return true;

            current->sleep_context.wakeup_ns = target_ns;
            current->sleep_context.interrupted = false;

            sleep_timer::notify_sleep(static_cast<u8>(cpu_id), target_ns);

            kernel::scheduling::add_blocked_unit(current, cpu_id);
            kernel::scheduling::yield();

            return !current->sleep_context.interrupted;
        }

        bool thread_sleep_ns(const u64 ns) {
            return thread_sleep_until_ns(get_uptime_ns() + ns);
        }

        void busy_sleep_until_ns(const u64 target_ns) {
            while (get_uptime_ns() < target_ns) {
                asm volatile("hlt");
            }
        }

        void busy_sleep_ns(const u64 ns) {
            busy_sleep_until_ns(get_uptime_ns() + ns);
        }
    } // namespace internal

    bool sleep_until_ns(const u64 target_ns) {
        if (scheduling::is_curent_cpu_enabled()) {
            return internal::thread_sleep_until_ns(target_ns);
        } else {
            internal::busy_sleep_until_ns(target_ns);
            return true;
        }
    }

    bool sleep_until_us(const u64 target_us) {
        return sleep_until_ns(target_us * 1'000ULL);
    }

    bool sleep_until_ms(const u64 target_ms) {
        return sleep_until_ns(target_ms * 1'000'000ULL);
    }

    void sleep_ms(const u64 ms) {
        sleep_until_ms((get_uptime_ns() / 1'000'000ULL) + ms);
    }

    void sleep_us(const u64 us) {
        sleep_until_us((get_uptime_ns() / 1'000ULL) + us);
    }

    bool sleep_ns(const u64 ns) {
        return sleep_until_ns(get_uptime_ns() + ns);
    }

    callback_timer::CallbackHandle schedule_callback(u64 deadline_ns, callback_timer::CallbackFn fn, void* arg) {
        return callback_timer::schedule(cpu_manager::get_current_cpu_id(), deadline_ns, fn, arg);
    }

    void cancel_callback(callback_timer::CallbackHandle h) {
        callback_timer::cancel(h);
    }
} // namespace kernel::time
