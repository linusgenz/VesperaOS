//
// Created by Linus on 17.07.25.
//
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <vespera/types.h>

#include "../../kernel/scheduling/reaper.h"
#include "../kernel/units/unit.h"

struct TrapFrame;

namespace kernel::scheduling::cpu_scheduler
{
    struct CpuScheduler {
        IntrusiveQueue<Unit> ready_queue;
        IntrusiveQueue<Unit> blocked_queue;
        Reaper reaper;
        Unit *current_unit;
        Unit *idle_unit;
        u32 quantum_ticks;
        u32 ticks_remaining;
        bool scheduler_enabled;
        bool need_resched;
        Spinlock lock;
    };
}

namespace kernel::scheduling {

    struct GlobalScheduler {
        cpu_scheduler::CpuScheduler cpus[kernel::acpi::madt::MAX_CPU_CORES];
        u32 num_cpus;
        bool initialized;
    };

    extern GlobalScheduler global_scheduler;

    // Global scheduler operations
    void init(u32 num_cpus);
    void yield();
 //   void tick();

    // Global thread management
    void add_unit(Unit* unit);
    void remove_unit(Unit* unit);
    void add_blocked_unit(Unit *unit, u8 cpu_id);

    // CPU management
    void enable_on_cpu(u8 cpu_id);
    void disable_on_cpu(u8 cpu_id);

    bool is_curent_cpu_enabled();

    // Query functions
    Unit* get_current_unit();
    bool is_initialized();
    u32 get_num_cpus();

    cpu_scheduler::CpuScheduler *get_cpu_data(u8 cpu_id);


    void wake_sleeping_units(u8 cpu_id, u64 current_tick);

    void tick_cpu(u8 cpu_id, TrapFrame *frame);
} // namespace kernel::scheduling::scheduler

#endif // SCHEDULER_H