#include "pit.h"
#include "../../cpu/io.h"

namespace PIT {
    double time_since_boot = 0;

    uint16_t divisor = 65535;

    void sleepd(double seconds) {
        double start_time = time_since_boot;
        while (time_since_boot < start_time + seconds) {
            asm("hlt");
        }
    }

    void pit_wait_ms(uint32_t ms) {
        uint32_t ticks = (base_frequency * ms) / 1000;
        asm volatile ("cli");
        outb(PIT_COMMAND, 0x34);            // Channel 0, mode 2, binary
        outb(PIT_CHANNEL0, ticks & 0xFF);
        outb(PIT_CHANNEL0, (ticks >> 8) & 0xFF);
        asm volatile("sti");

        for (;;) {
            uint8_t status = inb(0x61);
            if (status & 0x20) break;
        }
    }


    void sleep(uint64_t milliseconds) {
        sleepd(static_cast<double>(milliseconds) / 1000);
    }

    void set_divisor(uint16_t _divisor) {
        if (divisor < 100) _divisor = 100;
        divisor = _divisor;
        outb(PIT_CHANNEL0, static_cast<uint8_t>(divisor & 0x00ff));
        io_wait();
        outb(PIT_CHANNEL0, static_cast<uint8_t>((divisor & 0xff00) >> 8));
    }

    uint64_t get_frequency() {
        return base_frequency / divisor;
    }

    void set_frequency(uint64_t frequency) {
        set_divisor(base_frequency / frequency);
    }

    void tick() {
        time_since_boot += 1 / static_cast<double>(get_frequency());
    }
}