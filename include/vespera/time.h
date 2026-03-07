//
// Created by linus on 02.07.25.
//

#ifndef TIMER_H
#define TIMER_H
#include <vespera/types.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

namespace kernel::time {
    void sleep_ms(u64 ms);
    u64 get_ticks();
    u64 get_uptime_ms();


    void read_rtc(u8& second, u8& minute, u8& hour,
              u8& day, u8& month, u8& year);
    void print_current_time();

    namespace internal {
        void thread_sleep_ms(u64 ms);
        void sleep(u64 ms);
    }
}

#endif //TIMER_H
