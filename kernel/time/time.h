//
// Created by linus on 02.07.25.
//

#ifndef TIMER_H
#define TIMER_H
#include <stdint.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

namespace kernel::time {
    void sleep_ms(uint64_t ms);
    void thread_sleep_ms(uint64_t ms);
    uint64_t get_ticks();
    uint64_t get_uptime_ms();


    void read_rtc(uint8_t& second, uint8_t& minute, uint8_t& hour,
              uint8_t& day, uint8_t& month, uint8_t& year);
    void print_current_time();
}

#endif //TIMER_H
