#include "cpu_scheduler.h"

#include <vespera/scheduling.h>

#include "../../arch/x86_64/gdt/gdt.h"
#include "../../arch/x86_64/interrupts/apic.h"
#include "vespera/mm/memory.h"
#include "../units/unit_manager.h"
#include "arch/x86_64/cpu/msr.h"
#include "per_cpu.h"
#include "schedule_manager.h"
#include "vespera/log.h"
#include "vespera/realm/realm_manager.h"
#include "vespera/time.h"

GsData g_per_cpu[kernel::acpi::madt::MAX_CPU_CORES];

namespace kernel::scheduling::cpu_scheduler {
    CpuScheduler* get_cpu_data(const u8 cpu_id) {
        return &global_scheduler.cpus[cpu_id];
    }

    void init_cpu(const u8 cpu_id) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);

        // Setup idle unit für this CPU
        cpu->idle_unit = manager::setup_idle_unit(cpu_id);

        cpu->ready_queue.clear();
        cpu->blocked_queue.clear();
        cpu->current_unit = nullptr;
        cpu->quantum_ticks = SCHEDULER_TICKS;
        cpu->ticks_remaining = cpu->quantum_ticks;
        cpu->scheduler_enabled = false;
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "cpu%u", cpu_id);
        cpu->lock.init(buffer);

        const UnitConfig uc = {
            .name = "reaper_unit",
            .cpu_id = cpu_id,
            .priority = 5,
            .stack_size = DEFAULT_UNIT_STACK_SIZE,
            .initial_handles = nullptr,
            .initial_handle_count = 0,
            .is_idle = false,
            .is_user = false,
            .user_stack_size = 0
        };
        UnitManager::create(KERNEL_REALM_SYSTEM, reaper_unit, nullptr, &uc);
    }

    void enable_cpu(const u8 cpu_id) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);
        cpu->scheduler_enabled = true;
        cpu->ticks_remaining = cpu->quantum_ticks;
    }

    void disable_cpu(const u8 cpu_id) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);
        cpu->scheduler_enabled = false;
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

    static phys_addr_t realm_get_phys(Realm* realm, const uptr vaddr) {
        const uptr page_vaddr = vaddr & ~0xFFFULL;
        const uptr offset = vaddr & 0xFFFULL;

        const phys_addr_t phys_page = realm->page_table->get_physical_address(virt_from_raw(page_vaddr));
        if (phys_null(phys_page)) return make_phys(0);

        return phys_add(phys_page, offset);
    }

    bool once = false;
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

        // GS/KERNEL_GS invariant for isr_common_entry's conditional swapgs:
        //
        // We always leave MSRs in "kernel layout" here, regardless of whether next is a
        // user or kernel unit:
        //   GS_BASE        = GsData*   (kernel pointer)
        //   KERNEL_GS_BASE = 0         (user placeholder)
        //
        // isr_common_entry will swapgs before iretq if and only if CS has RPL=3.
        // That swapgs flips the two, resulting in the correct "user layout":
        //   GS_BASE        = 0         (user GS / future TLS)
        //   KERNEL_GS_BASE = GsData*   (restored by swapgs on next syscall/interrupt entry)
        //
        // For kernel units isr_common_entry skips swapgs, so GS_BASE stays as GsData* directly. :)
        wrmsr(MSR_GS_BASE, reinterpret_cast<u64>(&g_per_cpu[cpu_id]));
        if (next->is_user) {
            wrmsr(MSR_KERNEL_GS_BASE, 0);
        }

        if (next->is_user && next->rid) {
            if (next->parent) {
                u64 cr3 = phys_raw(next->parent->pml4_phys);
                asm volatile("mov %0, %%cr3" ::"r"(cr3) : "memory");
            } else {
                return;
            }
        } else {
            asm volatile("mov %0, %%cr3" ::"r"(kernel::memory::get_pagetable_address()) : "memory");
        }

        tss_set_rsp0(next->cpu_id, virt_raw(next->context.stack_pointer));

        wrmsr(MSR_FS_BASE, next->context.fs_base);
    }

    static Unit* pick_next(CpuScheduler* cpu) {
        Unit* next = cpu->ready_queue.pop();
        if (!next) next = cpu->idle_unit;
        return next;
    }

    void yield_cpu(u8 cpu_id, TrapFrame* tf) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);
        if (!cpu->scheduler_enabled) return;
        if (!tf) {
            return;
        }

        u64 flags = 0;
        cpu->lock.lock_irqsave(flags);

        Unit* prev = cpu->current_unit;
        bool prev_terminated = prev && (prev->state == UnitState::Terminated);
        bool prev_blocked = prev && (prev->state == UnitState::Blocked);
        bool prev_idle = prev && prev->is_idle;
        bool prev_can_requeue =
            prev && !prev_terminated && !prev_blocked && !prev_idle && prev->state == UnitState::Running;

        if (prev_can_requeue) {
            prev->state = UnitState::Ready;
            cpu->ready_queue.push(prev);
        }

        Unit* next = pick_next(cpu);

        if (next == prev && !prev_terminated && !prev_blocked) {
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

        cpu->lock.unlock();  // unlock before touching TrapFrame (irqs are already off)

        do_switch(prev, next, tf);
    }

    void tick_cpu(const u8 cpu_id, TrapFrame* frame) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);
        if (!cpu->scheduler_enabled) return;

        // Use atomic operations for tick counting to avoid locks
        bool should_yield = false;

        if (cpu->ticks_remaining > 0) {
            const u32 old_ticks = __sync_fetch_and_sub(&cpu->ticks_remaining, 1);
            should_yield = (old_ticks == 1);  // Was 1, now 0
        }

        // Check reschedule flag - use 0 instead of false
        if (__sync_lock_test_and_set(&cpu->need_resched, false)) {
            should_yield = true;
        }

        if (should_yield) {
            yield_cpu(cpu_id, frame);
        }
    }

    Unit* get_current_unit_on_cpu(const u8 cpu_id) {
        const CpuScheduler* cpu = get_cpu_data(cpu_id);
        return cpu->current_unit;
    }

    bool is_cpu_enabled(const u8 cpu_id) {
        const CpuScheduler* cpu = get_cpu_data(cpu_id);
        return cpu->scheduler_enabled;
    }

    // this declaration is irritating, as only units which should get waken up at a known time can be added here, using
    // sleep_context.wakeup_tick
    void add_blocked_unit(Unit* unit, const u8 cpu_id) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);

        cpu->ready_queue.remove(unit);  // current unit shouldn't be in ready_queue but just in case remove it anyway

        unit->next = nullptr;
        unit->state = UnitState::Blocked;

        cpu->blocked_queue.push(unit);

        if (unit == cpu->current_unit) {
            cpu->need_resched = true;
        }
    }

    void wake_sleeping_units(const u8 cpu_id) {
        CpuScheduler* cpu = get_cpu_data(cpu_id);

        Unit* woken = cpu->blocked_queue.extract_if([&](const Unit* unit) -> bool {
            return unit->sleep_context.wakeup_ns <= time::get_uptime_ns()|| unit->state == UnitState::Ready;;
        });

        while (woken) {
            Unit* next = woken->next;
            add_unit_to_cpu(woken, cpu_id);
            woken = next;
        }

        u64 new_min = 0;
        cpu->blocked_queue.for_each([&](const Unit* unit) {
            if (new_min == 0 || unit->sleep_context.wakeup_ns < new_min) {
                new_min = unit->sleep_context.wakeup_ns;
            }
        });
        time::sleep_timer::update_min_wakeup(cpu_id, new_min);
    }
}  // namespace kernel::scheduling::cpu_scheduler
