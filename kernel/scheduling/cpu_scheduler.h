//
// Created by Linus on 17.07.25.
//
#ifndef CPU_SCHEDULER_H
#define CPU_SCHEDULER_H

#include <cstddef>
#include "../acpi/madt.h"
#include "../../arch/x86_64/interrupts/interrupts_internal.h"
#include <intrusive_queue.h>

#define READY_SCAN_LIMIT 16

#define SCHEDULER_QUANTUM_MS 10   // Time slice in milliseconds
#define SCHEDULER_TICK_MS    10  // Interrupt frequency
#define SCHEDULER_TICKS (SCHEDULER_QUANTUM_MS / SCHEDULER_TICK_MS)

class Unit;

namespace kernel::scheduling::cpu_scheduler {
    struct cpu_scheduler_t {
        intrusive_queue_t<Unit, queue_lock_irq> ready_queue;
        intrusive_queue_t<Unit, queue_lock_irq> blocked_queue;
        Unit *current_unit;
        Unit *idle_unit;
        uint32_t quantum_ticks;
        uint32_t ticks_remaining;
        bool scheduler_enabled;
        bool need_resched;
        spinlock_t lock;
    };

    // Per-CPU operations
    void init_cpu(uint8_t cpu_id);

    void enable_cpu(uint8_t cpu_id);

    void disable_cpu(uint8_t cpu_id);

    // Thread queue management for specific CPU
    void add_unit_to_cpu(Unit *unit, uint8_t cpu_id);

    void remove_unit_from_cpu(Unit *unit, uint8_t cpu_id);

    // Context switching and execution
    void yield_cpu(uint8_t cpu_id, trap_frame *frame = nullptr);

    void tick_cpu(uint8_t cpu_id, trap_frame *frame);

    cpu_scheduler_t *get_cpu_data(uint8_t cpu_id);

    // Query functions
    Unit *get_current_unit_on_cpu(uint8_t cpu_id);

    bool is_cpu_enabled(uint8_t cpu_id);

    void add_blocked_unit(Unit *unit, uint8_t cpu_id);

    void wake_sleeping_units(uint8_t cpu_id, uint64_t current_tick);

  /*  static inline void lock_ready_queue(uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        while (__sync_lock_test_and_set(&cpu->ready_queue_lock, 1)) {
            asm volatile ("pause");
        }
    }

    static inline void unlock_ready_queue(uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        __sync_lock_release(&cpu->ready_queue_lock);
    }*/
/*
    static inline void lock_blocked_queue(uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        while (__sync_lock_test_and_set(&cpu->blocked_queue_lock, 1)) {
            asm volatile ("pause");
        }
    }

    static inline void unlock_blocked_queue(uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        __sync_lock_release(&cpu->blocked_queue_lock);
    }*/
} // namespace kernel::scheduling::cpu_scheduler

#endif // CPU_SCHEDULER_H