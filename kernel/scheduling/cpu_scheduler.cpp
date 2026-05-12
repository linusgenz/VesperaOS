
#include "cpu_scheduler.h"

#include <klib/string.h>
#include <vespera/scheduling.h>
#include <kernel/units/unit.h>

#include <arch/x86_64/gdt/gdt.h>
#include <arch/x86_64/interrupts/apic.h>
#include "../cpu/cpu_manager.h"
#include "../realm/address_space.h"
#include "../units/unit_manager.h"
#include "arch/x86_64/cpu/msr.h"
#include "per_cpu.h"
#include "vespera/log.h"
#include "vespera/mm/memory.h"
#include "vespera/realm/realm_manager.h"
#include "vespera/time.h"

GsData g_per_cpu[kernel::acpi::madt::MAX_CPU_CORES];

extern "C" [[noreturn]] void idle_unit_func(void* /*arg*/) {
    while (true) {
        __asm__ volatile("sti; hlt");
    }
}

/// Terminates the currently running unit on this CPU.
/// Marks it Terminated, removes it from all queues, hands it to the reaper,
/// then spins on HLT – the next tick/yield will switch to another unit.
[[noreturn]] static void terminate_current_unit() {
    asm volatile("cli");

    const u8 cpu_id = cpu_manager::get_current_cpu_id();
    kernel::scheduling::cpu_scheduler::CpuScheduler* cpu = kernel::scheduling::cpu_scheduler::get_cpu_data(cpu_id);
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

/// Entry trampoline for every new unit.
/// Reads the unit's entry/arg from its ExecutionContext and calls through.
/// When the entry function returns, the unit is terminated cleanly.
extern "C" [[noreturn]] void unit_trampoline() {
    const u8 cpu_id = cpu_manager::get_current_cpu_id();
    const Unit* u = kernel::scheduling::cpu_scheduler::get_current_unit_on_cpu(cpu_id);

    if (u && u->context.entry) {
        u->context.entry(u->context.arg);
    }

    Log::debug("unit_trampoline: unit %u finished", u ? u->id : 0u);
    terminate_current_unit();
}

/// Allocates and registers the idle unit for the given CPU.
/// Called once per CPU during scheduler initialization.
Unit* setup_idle_unit(const u8 cpu_id) {
    const UnitConfig cfg = {
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
    return UnitManager::create(KERNEL_REALM_SYSTEM, idle_unit_func, nullptr, &cfg);
}

namespace kernel::scheduling::cpu_scheduler {

    /// Saves prev's register state / FPU, loads next's state, updates CR3 and
    /// the MSRs that govern GS/FS and the kernel stack pointer in the TSS.
    ///
    /// GS / KERNEL_GS invariant
    /// ────────────────────────
    /// We always leave the MSRs in "kernel layout" before iretq:
    ///   GS_BASE        = GsData*   (kernel pointer)
    ///   KERNEL_GS_BASE = 0         (user placeholder)
    ///
    /// isr_common_entry swapgs before iretq iff CS has RPL=3, producing:
    ///   GS_BASE        = 0         (user GS / future TLS)
    ///   KERNEL_GS_BASE = GsData*   (restored on the next syscall/interrupt)
    ///
    /// For kernel units isr_common_entry skips swapgs, so GS_BASE stays as
    /// GsData* directly.
    static void do_switch(Unit* prev, Unit* next, TrapFrame* tf) {
        const u64 now = kernel::time::get_uptime_ns();

        if (prev) {
            if (prev->run_start_ns != 0) {
                prev->cpu_time_ns += now - prev->run_start_ns;
            }
            prev->context.fs_base = rdmsr(MSR_FS_BASE);
            cpu_context_save(tf, &prev->context.cpu_ctx);
            fpu_save(&prev->context.fpu_ctx);
        }

        next->run_start_ns = now;
        fpu_restore(&next->context.fpu_ctx);
        cpu_context_load(&next->context.cpu_ctx, tf);

        const u8 cpu_id = next->cpu_id;
        g_per_cpu[cpu_id].current_ctx = &next->context;

        wrmsr(MSR_GS_BASE, reinterpret_cast<u64>(&g_per_cpu[cpu_id]));
        if (next->is_user) {
            wrmsr(MSR_KERNEL_GS_BASE, 0);
        }

        if (next->is_user && next->rid) {
            if (!next->parent) return;  // should not happen
            const u64 cr3 = phys_raw(next->parent->address_space->pml4_phys());
            asm volatile("mov %0, %%cr3" ::"r"(cr3) : "memory");
        } else {
            asm volatile("mov %0, %%cr3" ::"r"(kernel::memory::get_pagetable_address()) : "memory");
        }

        tss_set_rsp0(next->cpu_id, virt_raw(next->context.stack_pointer));
        wrmsr(MSR_FS_BASE, next->context.fs_base);
    }

    CpuScheduler* get_cpu_data(const u8 cpu_id) {
        return &global_scheduler.cpus[cpu_id];
    }

    void init_cpu(const u8 cpu_id) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);

        cpu->idle_unit = setup_idle_unit(cpu_id);
        cpu->current_unit = nullptr;
        cpu->quantum_ticks = SCHEDULER_TICKS;
        cpu->ticks_remaining = cpu->quantum_ticks;
        cpu->scheduler_enabled = false;
        cpu->ready_queue.clear();
        cpu->blocked_queue.clear();

        char name[32];
        snprintf(name, sizeof(name), "cpu%u", cpu_id);
        cpu->lock.init(name);

        // Each CPU gets a reaper unit that periodically frees terminated units.
        const UnitConfig reaper_cfg = {
            .name = "reaper_unit",
            .cpu_id = cpu_id,
            .priority = 5,
            .stack_size = DEFAULT_UNIT_STACK_SIZE,
            .initial_handles = nullptr,
            .initial_handle_count = 0,
            .is_idle = false,
            .is_user = false,
            .user_stack_size = 0,
        };
        UnitManager::create(KERNEL_REALM_SYSTEM, reaper_unit, nullptr, &reaper_cfg);
    }

    void enable_cpu(const u8 cpu_id) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);
        cpu->scheduler_enabled = true;
        cpu->ticks_remaining = cpu->quantum_ticks;
    }

    void disable_cpu(const u8 cpu_id) {
        get_cpu_data(cpu_id)->scheduler_enabled = false;
    }

    void add_unit_to_cpu(Unit* unit, const u8 cpu_id) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);

        unit->state = UnitState::Ready;
        unit->next = nullptr;
        cpu->ready_queue.push(unit);

        if (cpu->scheduler_enabled && cpu->current_unit == cpu->idle_unit) {
            cpu->need_resched = true;
        }
    }

    void remove_unit_from_cpu(Unit* unit, const u8 cpu_id) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);
        cpu->ready_queue.remove(unit);
        unit->next = nullptr;
    }

    static Unit* pick_next(CpuScheduler* cpu) {
        Unit* next = cpu->ready_queue.pop();
        return next ? next : cpu->idle_unit;
    }

    void yield_cpu(const u8 cpu_id, TrapFrame* tf) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);
        if (!cpu->scheduler_enabled || !tf) return;

        u64 flags = 0;
        cpu->lock.lock_irqsave(flags);

        Unit* prev = cpu->current_unit;
        const bool terminated = prev && prev->state == UnitState::Terminated;
        const bool blocked = prev && prev->state == UnitState::Blocked;
        const bool idle = prev && prev->is_idle;
        const bool can_requeue = prev && !terminated && !blocked && !idle && prev->state == UnitState::Running;

        if (can_requeue) {
            prev->state = UnitState::Ready;
            cpu->ready_queue.push(prev);
        }

        Unit* next = pick_next(cpu);

        // Nothing changed – just reset the quantum and continue.
        if (next == prev && !terminated && !blocked) {
            prev->state = UnitState::Running;
            cpu->ticks_remaining = cpu->quantum_ticks;
            cpu->lock.unlock_irqrestore(flags);
            return;
        }

        next->state = UnitState::Running;
        cpu->current_unit = next;
        cpu->ticks_remaining = cpu->quantum_ticks;

        time::sleep_timer::set_quantum_deadline(
            cpu_id, time::get_uptime_ns() + arch::x86_64::interrupts::apic::APIC_QUANTUM_NS
        );

        // Unlock before touching the TrapFrame; IRQs are still off.
        cpu->lock.unlock();
        do_switch(prev, next, tf);
    }

    void tick_cpu(const u8 cpu_id, TrapFrame* frame) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);
        if (!cpu->scheduler_enabled) return;

        bool should_yield = false;

        // Decrement remaining ticks atomically; yield when the slice expires.
        if (cpu->ticks_remaining > 0) {
            const u32 prev = __sync_fetch_and_sub(&cpu->ticks_remaining, 1);
            should_yield = (prev == 1);
        }

        // Also yield if another path (e.g. add_unit_to_cpu) raised need_resched.
        if (__sync_lock_test_and_set(&cpu->need_resched, false)) {
            should_yield = true;
        }

        if (should_yield) {
            yield_cpu(cpu_id, frame);
        }
    }

    Unit* get_current_unit_on_cpu(const u8 cpu_id) {
        return get_cpu_data(cpu_id)->current_unit;
    }

    bool is_cpu_enabled(const u8 cpu_id) {
        return get_cpu_data(cpu_id)->scheduler_enabled;
    }

    void add_blocked_unit(Unit* unit, const u8 cpu_id) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);

        // Remove from ready queue first – current unit shouldn't be there,
        // but be defensive.
        cpu->ready_queue.remove(unit);
        unit->next = nullptr;
        unit->state = UnitState::Blocked;
        cpu->blocked_queue.push(unit);

        if (unit == cpu->current_unit) {
            cpu->need_resched = true;
        }
    }

    void wake_sleeping_units(const u8 cpu_id) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);

        // Extract all units whose sleep deadline has passed or that are already
        // marked Ready (e.g. woken by an event before the timer fired).
        Unit* woken = cpu->blocked_queue.extract_if([&](const Unit* u) -> bool {
            return u->sleep_context.wakeup_ns <= time::get_uptime_ns() || u->state == UnitState::Ready;
        });

        while (woken) {
            Unit* next = woken->next;
            add_unit_to_cpu(woken, cpu_id);
            woken = next;
        }

        // Recalculate the earliest remaining wakeup so the sleep timer can be
        // programmed accurately.
        u64 new_min = 0;
        cpu->blocked_queue.for_each([&](const Unit* u) {
            if (new_min == 0 || u->sleep_context.wakeup_ns < new_min) {
                new_min = u->sleep_context.wakeup_ns;
            }
        });
        time::sleep_timer::update_min_wakeup(cpu_id, new_min);
    }

}  // namespace kernel::scheduling::cpu_scheduler
