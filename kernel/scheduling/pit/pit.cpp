#include "pit.h"
#include "../../../drivers/io/io.h"

namespace PIT {
    double time_since_boot = 0;

    uint16_t divisor = 65535;

    void sleepd(double seconds) {
        double start_time = time_since_boot;
        while (time_since_boot < start_time + seconds) {
            asm("hlt");
        }
    }

    void sleep(uint64_t milliseconds) {
        sleepd((double)milliseconds / 1000);
    }

    void set_divisor(uint16_t _divisor) {
        if (divisor < 100) _divisor = 100;
        divisor = _divisor;
        outb(0x40, (uint8_t)(divisor & 0x00ff));
        io_wait();
        outb(0x40, (uint8_t)((divisor & 0xff00) >> 8));
    }

    uint64_t get_frequency() {
        return base_frequency / divisor;
    }

    void set_frequency(uint64_t frequency) {
        set_divisor(base_frequency / frequency);
    }

    void tick() {
        time_since_boot += 1 / (double)get_frequency();
    }
}