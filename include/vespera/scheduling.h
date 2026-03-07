//
// Created by Linus on 17.07.25.
//
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#include "../../kernel/scheduling/reaper.h"
#include "../kernel/units/unit.h"

struct TrapFrame;

namespace kernel::scheduling::cpu_scheduler
{
    struct CpuScheduler {
        IntrusiveQueue<Unit, QueueLockIrq> ready_queue;
        IntrusiveQueue<Unit, QueueLockIrq> blocked_queue;
        Reaper reaper;
        Unit *current_unit;
        Unit *idle_unit;
        uint32_t quantum_ticks;
        uint32_t ticks_remaining;
        bool scheduler_enabled;
        bool need_resched;
        Spinlock lock;
    };
}

namespace kernel::scheduling {

    struct GlobalScheduler {
        cpu_scheduler::CpuScheduler cpus[MAX_CPU_CORES];
        uint32_t num_cpus;
        bool initialized;
    };

    extern GlobalScheduler global_scheduler;

    // Global scheduler operations
    void init(uint32_t num_cpus);
    void yield();
 //   void tick();

    // Global thread management
    void add_unit(Unit* unit);
    void remove_unit(Unit* unit);
    void add_blocked_unit(Unit *unit, uint8_t cpu_id);

    // CPU management
    void enable_on_cpu(uint8_t cpu_id);
    void disable_on_cpu(uint8_t cpu_id);

    bool is_curent_cpu_enabled();

    // Query functions
    Unit* get_current_unit();
    bool is_initialized();
    uint32_t get_num_cpus();

    cpu_scheduler::CpuScheduler *get_cpu_data(uint8_t cpu_id);


    void wake_sleeping_units(uint8_t cpu_id, uint64_t current_tick);

    void tick_cpu(uint8_t cpu_id, TrapFrame *frame);
} // namespace kernel::scheduling::scheduler

#endif // SCHEDULER_H