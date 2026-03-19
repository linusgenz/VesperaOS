//
// Created by Linus on 17.07.25.
//

#include <vespera/scheduling.h>

#include "../cpu/cpu_manager.h"
#include "../units/unit.h"
#include "cpu_scheduler.h"

namespace kernel::scheduling {

    GlobalScheduler global_scheduler = {{}};

    void init(u32 num_cpus) {
        global_scheduler.num_cpus = num_cpus;
        global_scheduler.initialized = true;

        for (u32 i = 0; i < num_cpus; i++) {
            cpu_scheduler::init_cpu(i);
        }
    }

    void add_unit(Unit *unit) {
        if (!unit || !global_scheduler.initialized) return;

        const u8 cpu_id = unit->cpu_id;
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler::add_unit_to_cpu(unit, cpu_id);
    }

    void remove_unit(Unit *unit) {
        if (!unit || !global_scheduler.initialized) return;

        const u8 cpu_id = unit->cpu_id;
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler::remove_unit_from_cpu(unit, cpu_id);
    }

    void yield() {
        const u8 cpu_id = cpu_manager::get_current_cpu_id();
        cpu_scheduler::yield_cpu(cpu_id);
    }

    /*    void tick() {
            u8 cpu_id = CPUManager::get_current_cpu_id();
            cpu_scheduler::tick_cpu(cpu_id);
        }*/

    void enable_on_cpu(u8 cpu_id) {
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler::enable_cpu(cpu_id);
    }

    void disable_on_cpu(u8 cpu_id) {
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler::disable_cpu(cpu_id);
    }

    void add_blocked_unit(Unit *unit, u8 cpu_id) {
        cpu_scheduler::add_blocked_unit(unit, cpu_id);
    }

    bool is_curent_cpu_enabled() {
        const u8 cpu_id = cpu_manager::get_current_cpu_id();
        return cpu_scheduler::is_cpu_enabled(cpu_id);
    }

    Unit *get_current_unit() {
        const u32 cpu_id = cpu_manager::get_current_cpu_id();
        if (!global_scheduler.cpus[cpu_id].scheduler_enabled) return nullptr;
        return cpu_scheduler::get_current_unit_on_cpu(cpu_id);
    }

    bool is_initialized() {
        return global_scheduler.initialized;
    }

    u32 get_num_cpus() {
        return global_scheduler.num_cpus;
    }

    cpu_scheduler::CpuScheduler *get_cpu_data(u8 cpu_id) {
        return cpu_scheduler::get_cpu_data(cpu_id);
    }

    void wake_sleeping_units(u8 cpu_id, u64 current_tick) {
        cpu_scheduler::wake_sleeping_units(cpu_id, current_tick);
    }

    void tick_cpu(u8 cpu_id, TrapFrame *frame) {
        cpu_scheduler::tick_cpu(cpu_id, frame);
    }
}  // namespace kernel::scheduling
