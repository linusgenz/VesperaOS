//
// Created by Linus on 17.07.25.
//
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <cstdint>
#include "../kernel/units/unit.h"

struct trap_frame;

namespace kernel::scheduling::cpu_scheduler
{
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
}

namespace kernel::scheduling {

    struct global_scheduler_t {
        cpu_scheduler::cpu_scheduler_t cpus[MAX_CPU_CORES];
        uint32_t num_cpus;
        bool initialized;
    };

    extern global_scheduler_t global_scheduler;
    
    // Global scheduler operations
    void init(uint32_t num_cpus);
    void yield();
 //   void tick();

    // Global thread management
    void add_unit(Unit* thread);
    void remove_unit(Unit* thread);
    void add_blocked_unit(Unit *unit, uint8_t cpu_id);

    // CPU management
    void enable_on_cpu(uint8_t cpu_id);
    void disable_on_cpu(uint8_t cpu_id);

    bool is_curent_cpu_enabled();

    // Query functions
    Unit* get_current_unit();
    bool is_initialized();
    uint32_t get_num_cpus();

    cpu_scheduler::cpu_scheduler_t *get_cpu_data(uint8_t cpu_id);


    void wake_sleeping_units(uint8_t cpu_id, uint64_t current_tick);

    void tick_cpu(uint8_t cpu_id, trap_frame *frame);
} // namespace kernel::scheduling::scheduler

#endif // SCHEDULER_H