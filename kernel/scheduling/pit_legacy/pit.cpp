#include "pit.h"

#include "../../cpu/io.h"

namespace pit {
   /* double time_since_boot = 0;

    u16 divisor = 65535;

    void sleepd(double seconds) {
        double start_time = time_since_boot;
        while (time_since_boot < start_time + seconds) {
            asm("hlt");
        }
    }

    void pit_wait_ms(u32 ms) {
        u32 ticks = (BASE_FREQUENCY * ms) / 1000;
        asm volatile("cli");
        outb(PIT_COMMAND, 0x34);  // Channel 0, mode 2, binary
        outb(PIT_CHANNEL0, ticks & 0xFF);
        outb(PIT_CHANNEL0, (ticks >> 8) & 0xFF);
        asm volatile("sti");

        for (;;) {
            if (u8 status = inb(0x61); status & 0x20) break;
        }
    }

    void sleep(u64 milliseconds) {
        sleepd(static_cast<double>(milliseconds) / 1000);
    }

    void set_divisor(u16 _divisor) {
        if (divisor < 100) _divisor = 100;
        divisor = _divisor;
        outb(PIT_CHANNEL0, static_cast<u8>(divisor & 0x00ff));
        io_wait();
        outb(PIT_CHANNEL0, static_cast<u8>((divisor & 0xff00) >> 8));
    }

    u64 get_frequency() {
        return BASE_FREQUENCY / divisor;
    }

    void set_frequency(u64 frequency) {
        set_divisor(BASE_FREQUENCY / frequency);
    }

    void tick() {
        time_since_boot += 1 / static_cast<double>(get_frequency());
    }*/
}  // namespace PIT