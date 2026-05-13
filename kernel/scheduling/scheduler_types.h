#ifndef VESPERAOS_KERNEL_SCHEDULING_TYPES_H
#define VESPERAOS_KERNEL_SCHEDULING_TYPES_H

#include <acpi/madt.h>
#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

#include "reaper.h"
#include <units/unit.h>

namespace kernel::scheduling::cpu_scheduler {
    struct CpuScheduler {
        IntrusiveQueue<Unit> ready_queue;
        IntrusiveQueue<Unit> blocked_queue;
        Reaper reaper;
        Unit* current_unit;
        Unit* idle_unit;
        u32 quantum_ticks;
        u32 ticks_remaining;
        bool scheduler_enabled;
        bool need_resched;
        Spinlock lock;
    };
}

namespace kernel::scheduling {
    struct GlobalScheduler {
        cpu_scheduler::CpuScheduler cpus[acpi::madt::MAX_CPU_CORES];
        u32 num_cpus;
        bool initialized;
    };

    extern GlobalScheduler global_scheduler;
}

#endif  // VESPERAOS_KERNEL_SCHEDULING_TYPES_H
