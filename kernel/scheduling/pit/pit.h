//
// Created by linus on 13.10.24.
//

#ifndef PIT_H
#define PIT_H
#include <stdint.h>

namespace PIT {
    extern double time_since_boot;
    const uint64_t base_frequency = 1193182;

    void sleepd(double seconds);
    void sleep(uint64_t milliseconds);

    void set_divisor(uint16_t divisor);
    uint64_t get_frequency();
    void set_frequency(uint64_t frequency);
    void tick();
}

#endif //PIT_H