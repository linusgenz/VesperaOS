//
// Created by linus on 02.07.25.
//

#include <log.h>
#include <time.h>
#include "../../arch/x86_64/interrupts/apic.h"
#include "../cpu/cpu_manager.h"
#include "../include/interrupts.h"
#include <scheduling.h>

extern volatile uint64_t apic_ticks[MAX_CPU_CORES];

namespace kernel::time {
    namespace internal {
        void thread_sleep_ms(uint64_t ms) {
            uint32_t cpu_id = CPUManager::get_current_cpu_id();

            Unit *current = kernel::scheduling::get_current_unit();
            if (!current || current->is_idle) return;

            current->sleep_context.wakeup_tick = interrupts::lapic_get_ticks(cpu_id) + (ms + 9) / 10;
            kernel::scheduling::cpu_scheduler::add_blocked_unit(current, cpu_id);
            kernel::scheduling::yield();
        }

        void sleep(uint64_t ms) {
            uint32_t cpu = CPUManager::get_current_cpu_id();
            uint64_t target = interrupts::lapic_get_ticks(cpu) + (ms + 9) / 10;

            while (interrupts::lapic_get_ticks(cpu) < target) {
                asm volatile("hlt");
            }
        }
    }

    void sleep_ms(uint64_t ms) {
        Unit *current = kernel::scheduling::get_current_unit();
        if (kernel::scheduling::is_initialized() && current) {
            internal::thread_sleep_ms(ms);
        } else {
            internal::sleep(ms);
        }
    }

    uint64_t get_ticks() {
        uint32_t cpu = CPUManager::get_current_cpu_id();
        return interrupts::lapic_get_ticks(cpu);
    }

    uint64_t get_uptime_ms() {
        return get_ticks() * 10;
    }
}
