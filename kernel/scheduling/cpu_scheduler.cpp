#include "cpu_scheduler.h"

#include <kernel/scheduling.h>
#include <log.h>

#include "../utils/panic.h"
#include "schedule_manager.h"

namespace kernel::scheduling::cpu_scheduler {
    cpu_scheduler_t* get_cpu_data(const uint8_t cpu_id) {
        return &global_scheduler.cpus[cpu_id];
    }

    void init_cpu(const uint8_t cpu_id) {
        cpu_scheduler_t* cpu = get_cpu_data(cpu_id);

        // Setup idle unit für this CPU
        cpu->idle_unit = manager::setup_idle_unit(cpu_id);

        cpu->ready_queue.clear();
        cpu->blocked_queue.clear();
        cpu->current_unit = nullptr;
        cpu->quantum_ticks = SCHEDULER_TICKS;
        cpu->ticks_remaining = cpu->quantum_ticks;
        cpu->scheduler_enabled = false;
        cpu->lock.init();

        //  if (cpu_id == 2 || cpu_id == 4 || cpu_id == 5 || cpu_id == 6 || cpu_id == 7 || cpu_id == 1)
        {
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
    }

    void enable_cpu(const uint8_t cpu_id) {
        cpu_scheduler_t* cpu = get_cpu_data(cpu_id);
        cpu->scheduler_enabled = true;
        cpu->ticks_remaining = cpu->quantum_ticks;

        yield_cpu(cpu_id);
    }

    void disable_cpu(const uint8_t cpu_id) {
        cpu_scheduler_t* cpu = get_cpu_data(cpu_id);
        cpu->scheduler_enabled = false;
    }

    void add_unit_to_cpu(Unit* unit, const uint8_t cpu_id) {
        cpu_scheduler_t* cpu = get_cpu_data(cpu_id);

        unit->state = UNIT_READY;
        unit->next = nullptr;

        cpu->ready_queue.push(unit);

        if (cpu->scheduler_enabled && cpu->current_unit == cpu->idle_unit) {
            cpu->need_resched = true;
        }
    }

    void remove_unit_from_cpu(Unit* unit, const uint8_t cpu_id) {
        cpu_scheduler_t* cpu = get_cpu_data(cpu_id);

        cpu->ready_queue.remove(unit);
        unit->next = nullptr;
    }

    void yield_cpu(const uint8_t cpu_id, trap_frame* frame) {
        cpu_scheduler_t* cpu = get_cpu_data(cpu_id);
        if (!cpu->scheduler_enabled) return;

        uint64_t flags = 0;
        cpu->lock.lock_irqsave(flags);

        Unit* current = cpu->current_unit;

        // Special case: no current thread
        if (current == nullptr) {
            Unit* next_unit = cpu->ready_queue.pop();
            if (!next_unit) {
                next_unit = cpu->idle_unit;
            }
            next_unit->state = UNIT_RUNNING;
            cpu->current_unit = next_unit;
            cpu->ticks_remaining = cpu->quantum_ticks;
            cpu->lock.unlock_irqrestore(flags);
            manager::switch_to_unit(nullptr, next_unit, frame);
            return;
        }

        const bool current_terminated = (current->state == UNIT_TERMINATED);
        const bool current_blocked = (current->state == UNIT_BLOCKED);
        const bool current_is_idle = current->is_idle;
        const bool current_can_continue = (!current_terminated && !current_blocked && current->state == UNIT_RUNNING);

        // Get next unit from ready queue
        Unit* next_unit = cpu->ready_queue.pop();

        if (next_unit == current) {
            panic("yield_cpu: current thread was in ready queue!");
            next_unit = cpu->ready_queue.pop();
        }

        // Case 1: Idle is running and nothing to do
        if (!next_unit && current_is_idle) {
            cpu->ticks_remaining = cpu->quantum_ticks;
            cpu->lock.unlock_irqrestore(flags);
            return;
        }

        // Case 2: Current is terminated or blocked -> MUST switch
        if (current_terminated || current_blocked) {
            if (!next_unit) {
                next_unit = cpu->idle_unit;
            }
            next_unit->state = UNIT_RUNNING;
            cpu->current_unit = next_unit;
            cpu->ticks_remaining = cpu->quantum_ticks;
            cpu->lock.unlock_irqrestore(flags);
            manager::switch_to_unit(current, next_unit, frame);
            return;
        }

        // Case 3: Current is idle and we have a real thread waiting
        if (current_is_idle && next_unit && !next_unit->is_idle) {
            next_unit->state = UNIT_RUNNING;
            cpu->current_unit = next_unit;
            cpu->ticks_remaining = cpu->quantum_ticks;
            cpu->lock.unlock_irqrestore(flags);
            manager::switch_to_unit(current, next_unit, frame);
            return;
        }

        // Case 4: No next thread available
        if (!next_unit) {
            // Current continues running
            if (current_can_continue) {
                cpu->ticks_remaining = cpu->quantum_ticks;
                cpu->lock.unlock_irqrestore(flags);
                return;
            }

            // Current can't continue -> go to idle
            next_unit = cpu->idle_unit;
            next_unit->state = UNIT_RUNNING;
            cpu->current_unit = next_unit;
            cpu->ticks_remaining = cpu->quantum_ticks;
            cpu->lock.unlock_irqrestore(flags);
            manager::switch_to_unit(current, next_unit, frame);
            return;
        }

        // Case 6: We have a real next thread -> ALWAYS switch
        // This is the normal case for yield() and time slice expiration
        if (next_unit && next_unit != current) {
            // Re-queue current if it can continue
            if (current_can_continue && !current_is_idle) {
                cpu->ready_queue.remove(current);

                current->state = UNIT_READY;
                cpu->ready_queue.push(current);
            }

            next_unit->state = UNIT_RUNNING;
            cpu->current_unit = next_unit;
            cpu->ticks_remaining = cpu->quantum_ticks;
            cpu->lock.unlock_irqrestore(flags);
            manager::switch_to_unit(current, next_unit, frame);
            return;
        }

        // Fallback: continue with current
        cpu->ticks_remaining = cpu->quantum_ticks;
        cpu->lock.unlock_irqrestore(flags);
    }

    void tick_cpu(const uint8_t cpu_id, trap_frame* frame) {
        cpu_scheduler_t* cpu = get_cpu_data(cpu_id);
        if (!cpu->scheduler_enabled) return;

        // Use atomic operations for tick counting to avoid locks
        bool should_yield = false;

        if (cpu->ticks_remaining > 0) {
            uint32_t old_ticks = __sync_fetch_and_sub(&cpu->ticks_remaining, 1);
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

    Unit* get_current_unit_on_cpu(const uint8_t cpu_id) {
        cpu_scheduler_t* cpu = get_cpu_data(cpu_id);
        return cpu->current_unit;
    }

    bool is_cpu_enabled(const uint8_t cpu_id) {
        cpu_scheduler_t* cpu = get_cpu_data(cpu_id);
        return cpu->scheduler_enabled;
    }

    // this declaration is irritating, as only units which should get waken up at a known time can be added here, using
    // sleep_context.wakeup_tick
    void add_blocked_unit(Unit* unit, const uint8_t cpu_id) {
        cpu_scheduler_t* cpu = get_cpu_data(cpu_id);

        cpu->ready_queue.remove(unit);  // current unit shouldn't be in ready_queue but just in case remove it anyway

        unit->next = nullptr;
        unit->state = UNIT_BLOCKED;

        cpu->blocked_queue.push(unit);

        if (unit == cpu->current_unit) {
            cpu->need_resched = true;
        }
    }

    void wake_sleeping_units(const uint8_t cpu_id, const uint64_t current_tick) {
        cpu_scheduler_t* cpu = get_cpu_data(cpu_id);

        Unit* woken = cpu->blocked_queue.extract_if([&](const Unit* unit) -> bool {
            return current_tick >= unit->sleep_context.wakeup_tick;
        });

        while (woken) {
            Unit* next = woken->next;
            add_unit_to_cpu(woken, cpu_id);
            woken = next;
        }
    }
}  // namespace kernel::scheduling::cpu_scheduler
