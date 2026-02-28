//
// Created by linus on 13.10.24.
//

#ifndef PIT_H
#define PIT_H
#include <cstdint>

namespace PIT {
    extern double time_since_boot;
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND 0x43
    constexpr uint64_t base_frequency = 1193182;

    void sleepd(double seconds);
    void sleep(uint64_t milliseconds);

    void set_divisor(uint16_t divisor);
    uint64_t get_frequency();
    void set_frequency(uint64_t frequency);
    void tick();

    void pit_wait_ms(uint32_t ms);
}  // namespace PIT

#endif  // PIT_H