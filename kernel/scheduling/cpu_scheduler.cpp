#include "cpu_scheduler.h"

#include <scheduling.h>
#include <time.h>

#include "schedule_manager.h"
#include "../../include/log.h"

namespace kernel::scheduling::cpu_scheduler {
    cpu_scheduler_t *get_cpu_data(uint8_t cpu_id) {
        return &global_scheduler.cpus[cpu_id];
    }

    void init_cpu(uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);

        // Setup idle unit für diesen CPU
        cpu->idle_unit = manager::setup_idle_unit(cpu_id);

        // cpu->ready_queue_head = nullptr;
        //  cpu->ready_queue_tail = nullptr;
        // cpu->blocked_queue_head = nullptr;
        cpu->ready_queue.clear();
        cpu->blocked_queue.clear();
        cpu->current_unit = cpu->idle_unit;
        cpu->quantum_ticks = SCHEDULER_TICKS;
        cpu->ticks_remaining = cpu->quantum_ticks;
        cpu->scheduler_enabled = false;
        //  cpu->ready_queue_lock = 0;
        //  cpu->blocked_queue_lock = 0;
        cpu->scheduler_lock = 0;
    }


    void enable_cpu(const uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        cpu->scheduler_enabled = true;
        cpu->ticks_remaining = cpu->quantum_ticks;

        manager::switch_to_unit(nullptr, cpu->idle_unit, nullptr);
    }

    void disable_cpu(const uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        cpu->scheduler_enabled = false;
    }

    void add_unit_to_cpu(Unit *unit, const uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);

        unit->state = UNIT_READY;
        unit->next = nullptr;

        cpu->ready_queue.push(unit);

        if (cpu->scheduler_enabled && cpu->current_unit == cpu->idle_unit) {
            cpu->need_resched = true;
        }
    }

    void remove_unit_from_cpu(Unit *unit, const uint8_t cpu_id) {
        /* lock_ready_queue(cpu_id);

         cpu_scheduler_t *cpu = get_cpu_data(cpu_id);

         Unit *prev = nullptr;
         Unit *cur = cpu->ready_queue_head;
         while (cur && cur != unit) {
             prev = cur;
             cur = cur->next;
         }

         if (!cur) {
             unlock_ready_queue(cpu_id);
             return;
         }

         if (prev) prev->next = cur->next;
         else cpu->ready_queue_head = cur->next;

         if (cpu->ready_queue_tail == cur) cpu->ready_queue_tail = prev;

         unit->state = UNIT_TERMINATED;
         unit->next = nullptr;

         unlock_ready_queue(cpu_id);*/
    }

    void yield_cpu(uint8_t cpu_id, interrupt_frame *frame) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        if (!cpu->scheduler_enabled) return;
        // cpu->ready_queue.validate();

        // Lock scheduler state first
        while (__sync_lock_test_and_set(&cpu->scheduler_lock, 1)) {
            asm volatile ("pause");
        }

        Unit *current = cpu->current_unit;
        Unit *next_unit = cpu->ready_queue.pop();

        if (!next_unit) {
            if (current->is_idle) {
                __sync_lock_release(&cpu->scheduler_lock);
                return;
            }
            next_unit = cpu->idle_unit;
        }

        // if current got terminated continue with next
        if (current == nullptr) {
            next_unit->state = UNIT_RUNNING;
            cpu->current_unit = next_unit;
            cpu->ticks_remaining = cpu->quantum_ticks;

            __sync_lock_release(&cpu->scheduler_lock);
            manager::switch_to_unit(nullptr, next_unit, frame);
            return;
        }

        bool current_terminated = (current->state == UNIT_TERMINATED);
        bool current_blocked = (current->state == UNIT_BLOCKED);
        bool current_should_continue = (!current_terminated && !current_blocked &&
                                        !current->is_idle && current->state == UNIT_RUNNING);

        // If current thread is the only thread on the core
        if (!next_unit || next_unit->is_idle) {
            if (!current || current_terminated || current_blocked) {
                next_unit = cpu->idle_unit;
            } else {
                cpu->ticks_remaining = cpu->quantum_ticks;
                __sync_lock_release(&cpu->scheduler_lock);
                return;
            }
        }

        if (current_should_continue) {
            cpu->ticks_remaining = cpu->quantum_ticks;
            __sync_lock_release(&cpu->scheduler_lock);
            return;
        }


        // Switch to next thread
        if (next_unit && next_unit != current) {
            current->state = UNIT_READY;

            next_unit->state = UNIT_RUNNING;
            cpu->current_unit = next_unit;
            cpu->ticks_remaining = cpu->quantum_ticks;

            __sync_lock_release(&cpu->scheduler_lock);
            // Delegate actual context switch to thread_manager
            manager::switch_to_unit(current, next_unit, frame);
        } else {
            __sync_lock_release(&cpu->scheduler_lock);
        }
    }

    void tick_cpu(uint8_t cpu_id, interrupt_frame *frame) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        if (!cpu->scheduler_enabled) return;

        // Use atomic operations for tick counting to avoid locks
        bool should_yield = false;
        if (cpu->ticks_remaining > 0) {
            cpu->ticks_remaining = __sync_sub_and_fetch(&cpu->ticks_remaining, 1);
            should_yield = (cpu->ticks_remaining == 0);
        }

        if (__sync_lock_test_and_set(&cpu->need_resched, false)) {
            should_yield = true;
        }

        if (should_yield) {
            yield_cpu(cpu_id, frame);
        }
    }

    Unit *get_current_unit_on_cpu(const uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        return cpu->current_unit;
    }

    bool is_cpu_enabled(const uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        return cpu->scheduler_enabled;
    }

    void add_blocked_unit(Unit *unit, const uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);

        cpu->ready_queue.remove(unit);

        unit->next = nullptr;
        unit->state = UNIT_BLOCKED;

        cpu->blocked_queue.push(unit);
    }

    void wake_sleeping_units(uint8_t cpu_id, uint64_t current_tick) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);

        Unit *woken = cpu->blocked_queue.extract_if(
            [&](Unit *unit) -> bool {
                return current_tick >= unit->sleep_context.wakeup_tick;
            }
        );

        while (woken) {
            Unit *next = (Unit *) woken->next;
            add_unit_to_cpu(woken, cpu_id);
            woken = next;
        }
    }
}
