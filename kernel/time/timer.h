//
// Created by linus on 02.07.25.
//

#ifndef TIMER_H
#define TIMER_H
#include <stdint.h>

namespace kernel::time {
    void init_timer();
    void sleep_ms(uint64_t ms);
    uint64_t get_ticks();
    uint64_t get_uptime_ms();
}

#endif //TIMER_H
