//
// Created by Linus on 17.07.25.
//

#include <kernel/scheduling.h>

#include "cpu_scheduler.h"
#include "../cpu/cpu_manager.h"
#include "../units/unit.h"

namespace kernel::scheduling {

    global_scheduler_t global_scheduler = {{}};

    void init(uint32_t num_cpus) {
        global_scheduler.num_cpus = num_cpus;
        global_scheduler.initialized = true;

        for (uint32_t i = 0; i < num_cpus; i++) {
            cpu_scheduler::init_cpu(i);
        }
    }

    void add_unit(Unit *unit) {
        if (!unit || !global_scheduler.initialized) return;

        const uint8_t cpu_id = unit->cpu_id;
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler::add_unit_to_cpu(unit, cpu_id);
    }

    void remove_unit(Unit *unit) {
        if (!unit || !global_scheduler.initialized) return;

        uint8_t cpu_id = unit->cpu_id;
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler::remove_unit_from_cpu(unit, cpu_id);
    }

    void yield() {
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        cpu_scheduler::yield_cpu(cpu_id);
    }

    /*    void tick() {
            uint8_t cpu_id = CPUManager::get_current_cpu_id();
            cpu_scheduler::tick_cpu(cpu_id);
        }*/

    void enable_on_cpu(uint8_t cpu_id) {
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler::enable_cpu(cpu_id);
    }

    void disable_on_cpu(uint8_t cpu_id) {
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler::disable_cpu(cpu_id);
    }

    void add_blocked_unit(Unit *unit, uint8_t cpu_id)
    {
        cpu_scheduler::add_blocked_unit(unit, cpu_id);
    }

    bool is_curent_cpu_enabled() {
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        return cpu_scheduler::is_cpu_enabled(cpu_id);
    }

    Unit *get_current_unit() {
        const uint32_t cpu_id = CPUManager::get_current_cpu_id();
        if (!global_scheduler.cpus[cpu_id].scheduler_enabled) return nullptr;
        return cpu_scheduler::get_current_unit_on_cpu(cpu_id);
    }

    bool is_initialized() {
        return global_scheduler.initialized;
    }

    uint32_t get_num_cpus() {
        return global_scheduler.num_cpus;
    }

    cpu_scheduler::cpu_scheduler_t *get_cpu_data(uint8_t cpu_id)
    {
        return cpu_scheduler::get_cpu_data(cpu_id);
    }

    void wake_sleeping_units(uint8_t cpu_id, uint64_t current_tick)
    {
        cpu_scheduler::wake_sleeping_units(cpu_id, current_tick);
    }

    void tick_cpu(uint8_t cpu_id, trap_frame *frame)
    {
        cpu_scheduler::tick_cpu(cpu_id, frame);
    }
} // namespace kernel::scheduling::scheduler
