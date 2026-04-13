//
// Created by linus on 02.07.25.
//

#include <vespera/interrupts.h>
#include <vespera/scheduling.h>
#include <vespera/time.h>

#include "../cpu/cpu_manager.h"

extern volatile u64 apic_ticks[kernel::acpi::madt::MAX_CPU_CORES];

namespace kernel::time {
    namespace internal {
        void thread_sleep_ms(const u64 ms) {
            const u32 cpu_id = cpu_manager::get_current_cpu_id();

            Unit *current = kernel::scheduling::get_current_unit();
            if (!current || current->is_idle) return;

            current->sleep_context.wakeup_tick = interrupts::lapic_get_ticks(cpu_id) + (ms + 9) / 10;
            kernel::scheduling::add_blocked_unit(current, cpu_id);
            kernel::scheduling::yield();
        }

        void sleep(u64 ms) {
            u32 cpu = cpu_manager::get_current_cpu_id();
            u64 target = interrupts::lapic_get_ticks(cpu) + (ms + 9) / 10;

            while (interrupts::lapic_get_ticks(cpu) < target) {
                asm volatile("hlt");
            }
        }
    }  // namespace internal

    void sleep_ms(const u64 ms) {
        if (const Unit *current = scheduling::get_current_unit(); scheduling::is_initialized() && current) {
            internal::thread_sleep_ms(ms);
        } else {
            internal::sleep(ms);
        }
    }

    u64 get_ticks() {
        const u32 cpu = cpu_manager::get_current_cpu_id();
        return interrupts::lapic_get_ticks(cpu);
    }

    u64 get_uptime_ms() {
        return get_ticks() * 10;
    }
}  // namespace kernel::time
