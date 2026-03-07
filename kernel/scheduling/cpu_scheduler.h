//
// Created by Linus on 17.07.25.
//
#ifndef CPU_SCHEDULER_H
#define CPU_SCHEDULER_H

#include <vespera/scheduling.h>

#include "../../arch/x86_64/interrupts/interrupts_internal.h"

#define READY_SCAN_LIMIT 16

#define SCHEDULER_QUANTUM_MS 10  // Time slice in milliseconds
#define SCHEDULER_TICK_MS 10     // Interrupt frequency
#define SCHEDULER_TICKS (SCHEDULER_QUANTUM_MS / SCHEDULER_TICK_MS)

class Unit;

namespace kernel::scheduling::cpu_scheduler {

    // Per-CPU operations
    void init_cpu(uint8_t cpu_id);

    void enable_cpu(uint8_t cpu_id);

    void disable_cpu(uint8_t cpu_id);

    // Thread queue management for specific CPU
    void add_unit_to_cpu(Unit *unit, uint8_t cpu_id);

    void remove_unit_from_cpu(Unit *unit, uint8_t cpu_id);

    // Context switching and execution
    void yield_cpu(uint8_t cpu_id, TrapFrame *frame = nullptr);

    void tick_cpu(uint8_t cpu_id, TrapFrame *frame);

    CpuScheduler *get_cpu_data(uint8_t cpu_id);

    // Query functions
    Unit *get_current_unit_on_cpu(uint8_t cpu_id);

    bool is_cpu_enabled(uint8_t cpu_id);

    void add_blocked_unit(Unit *unit, uint8_t cpu_id);

    void wake_sleeping_units(uint8_t cpu_id, uint64_t current_tick);

}  // namespace kernel::scheduling::cpu_scheduler

#endif  // CPU_SCHEDULER_H