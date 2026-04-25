//
// Created by linus on 02.07.25.
//

#ifndef TIMER_H
#define TIMER_H
#include <vespera/types.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

namespace kernel::time {

    void init_clock();
    void init_wall_clock(u64 unix_epoch_ns);

    // Nanoseconds elapsed since the active clock source was initialized (~boot).
    [[nodiscard]] u64 get_uptime_ns();

    // Microseconds since boot.
    [[nodiscard]] u64 get_uptime_us();

    // Milliseconds since boot.
    [[nodiscard]] u64 get_uptime_ms();

    // Raw ticks from the active clock source (unit depends on source).
    // Use get_uptime_ns() / get_uptime_ms() for portable time comparisons.
    [[nodiscard]] u64 get_ticks();

    // Name of the clock source currently in use ("HPET", "APIC", "PIT", …).
    [[nodiscard]] const char* clock_source_name();

    // Sleep for at least ms milliseconds.
    void sleep_ms(u64 ms);

    // Sleep for at least us microsecond.
    void sleep_us(const u64 us);

    // Sleep for at least ns nanoseconds.
    bool sleep_ns(const u64 ns);

    u64 get_realtime_ns();

    // Read current wall-clock time from the CMOS RTC.
    // All output values are in binary (BCD is converted internally).
    void read_rtc(u8& second, u8& minute, u8& hour, u8& day, u8& month, u8& year);

    void epoch_init();

    namespace sleep_timer {

        void start(u8 cpu_id);

        void arm_next_event(u8 cpu_id);

        void notify_sleep(u8 cpu_id, u64 wakeup_ns);

        void update_min_wakeup(u8 cpu_id, u64 new_min_ns);

        void set_quantum_deadline(u8 cpu_id, u64 deadline_ns);

    }  // namespace sleep_timer
}  // namespace kernel::time

#endif  // TIMER_H
