//
// Created by Linus on 17.07.25.
//

#include "schedule_manager.h"

#include <vespera/realm/realm_manager.h>

#include "../../arch/x86_64/gdt/gdt.h"
#include <vespera/log.h>
#include "../cpu/cpu_manager.h"
#include "../units/unit_manager.h"
#include "cpu_scheduler.h"
#include <vespera/mm/memory.h>

namespace kernel::scheduling::manager {
    extern "C" [[noreturn]] void idle_unit_func(void *arg) {
        while (true) {
            __asm__ volatile("sti; hlt");
        }
    }

    [[noreturn]] void terminate_current_unit() {
        asm volatile("cli");
        const u8 cpu_id = cpu_manager::get_current_cpu_id();
        cpu_scheduler::CpuScheduler * cpu = cpu_scheduler::get_cpu_data(cpu_id);

        Unit* u = cpu->current_unit;
        if (u && !u->is_idle) {
            u->state = UnitState::Terminated;
            cpu->ready_queue.remove(u);
            cpu->blocked_queue.remove(u);
            cpu->reaper.enqueue(u);
        }

        cpu->need_resched = true;
        asm volatile("sti");
        while (true) asm volatile("hlt");
    }

    extern "C" [[noreturn]] void unit_trampoline() {
        const u8 cpu_id = cpu_manager::get_current_cpu_id();
        const Unit* u   = cpu_scheduler::get_current_unit_on_cpu(cpu_id);
        if (u && u->context.entry) {
            u->context.entry(u->context.arg);
        }
        Log::debug("unit_trampoline: unit %u finished", u ? u->id : 0);
        terminate_current_unit();
    }

    Unit *setup_idle_unit(const u8 cpu_id) {
        const UnitConfig unit_config = {
            .name = "idle_thread",
            .cpu_id = cpu_id,
            .priority = PRIORITY_NONE,
            .stack_size = 0x1000,
            .initial_handles = nullptr,
            .initial_handle_count = 0,
            .is_idle = true,
            .is_user = false,
            .user_stack_size = 0,
        };

        return UnitManager::create(KERNEL_REALM_SYSTEM, idle_unit_func, nullptr, &unit_config);
    }
}  // namespace kernel::scheduling::manager
