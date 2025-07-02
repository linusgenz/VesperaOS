//
// Created by linus on 02.07.25.
//
#include "timer.h"
#include "../../arch/x86_64/interrupts/apic.h"

extern volatile uint64_t apic_ticks;

namespace kernel::time {
    void init_timer() {
        lapic_init();
    }

    void sleep_ms(uint64_t ms) {
        uint64_t target = apic_ticks + (ms + 9) / 10;
        while (apic_ticks < target) {
            asm volatile("hlt");
        }
    }

    uint64_t get_ticks() {
        return apic_ticks;
    }

    uint64_t get_uptime_ms() {
        return apic_ticks * 10;
    }
}