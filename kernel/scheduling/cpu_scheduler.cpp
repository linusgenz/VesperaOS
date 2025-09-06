#include "cpu_scheduler.h"

#include <scheduling.h>
#include "../cpu/cpu_manager.h"
#include "thread_manager.h"
#include "../../arch/x86_64/gdt/gdt.h"
#include "../../include/log.h"

namespace kernel::scheduling::cpu_scheduler {

    cpu_scheduler_t *get_cpu_data(uint8_t cpu_id) {
        return &global_scheduler.cpus[cpu_id];
    }

    void init_cpu(uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);

        // Setup idle thread for this CPU
        thread_manager::setup_idle_thread(cpu_id);

        cpu->ready_queue_head = nullptr;
        cpu->ready_queue_tail = nullptr;
        cpu->blocked_queue_head = nullptr;
        cpu->current_thread = cpu->idle_thread;
        cpu->quantum_ticks = SCHEDULER_TICKS;
        cpu->ticks_remaining = cpu->quantum_ticks;
        cpu->scheduler_enabled = false;
        cpu->ready_queue_lock = 0;
        cpu->blocked_queue_lock = 0;
        cpu->scheduler_lock = 0;
    }

    void enable_cpu(uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        cpu->scheduler_enabled = true;
        cpu->ticks_remaining = cpu->quantum_ticks;

        thread_manager::switch_to_thread(nullptr, cpu->idle_thread, nullptr);
    }

    void disable_cpu(uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        cpu->scheduler_enabled = false;
    }

    void add_thread_to_cpu(kthread_t *thread, uint8_t cpu_id) {
        lock_ready_queue(cpu_id);
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);

        thread->state = THREAD_READY;
        thread->next = nullptr;

        if (cpu->ready_queue_tail) {
            cpu->ready_queue_tail->next = thread;
            cpu->ready_queue_tail = thread;
        } else {
            cpu->ready_queue_head = thread;
            cpu->ready_queue_tail = thread;
        }

        unlock_ready_queue(cpu_id);
    }

    void remove_thread_from_cpu(kthread_t *thread, uint8_t cpu_id) {
        lock_ready_queue(cpu_id);
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);

        if (cpu->ready_queue_head == thread) {
            cpu->ready_queue_head = thread->next;
            if (cpu->ready_queue_tail == thread) {
                cpu->ready_queue_tail = nullptr;
            }
        } else {
            kthread_t *prev = cpu->ready_queue_head;
            while (prev && prev->next != thread) {
                prev = prev->next;
            }
            if (prev) {
                prev->next = thread->next;
                if (cpu->ready_queue_tail == thread) {
                    cpu->ready_queue_tail = prev;
                }
            }
        }

        thread->state = THREAD_TERMINATED;
        thread->next = nullptr;

        unlock_ready_queue(cpu_id);
    }

    void yield_cpu(uint8_t cpu_id, interrupt_frame *frame) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        if (!cpu->scheduler_enabled) return;

        // Lock scheduler state first
        while (__sync_lock_test_and_set(&cpu->scheduler_lock, 1)) {
            asm volatile ("pause");
        }

        kthread_t *current = cpu->current_thread;
        kthread_t *next_thread = nullptr;

        // Get next thread from ready queue (separate lock)
        lock_ready_queue(cpu_id);
        if (cpu->ready_queue_head) {
            next_thread = cpu->ready_queue_head;
            cpu->ready_queue_head = next_thread->next;
            if (cpu->ready_queue_tail == next_thread) {
                cpu->ready_queue_tail = nullptr;
            }
            next_thread->next = nullptr;
        }
        unlock_ready_queue(cpu_id);

        if (!next_thread) {
            if (current->is_idle_thread) {
                __sync_lock_release(&cpu->scheduler_lock);
                return;
            }
            next_thread = cpu->idle_thread;
        }

        // if current got terminated continue with next
        if (current == nullptr) {
            next_thread->state = THREAD_RUNNING;
            cpu->current_thread = next_thread;
            cpu->ticks_remaining = cpu->quantum_ticks;

            __sync_lock_release(&cpu->scheduler_lock);
            thread_manager::switch_to_thread(nullptr, next_thread, frame);
            return;
        }

        bool current_terminated = (current->state == THREAD_TERMINATED);
        bool current_blocked = (current->state == THREAD_BLOCKED);
        bool current_should_continue = ( !current_terminated && !current_blocked &&
                                        !current->is_idle_thread && current->state == THREAD_RUNNING);

        // If current thread is the only thread on the core
        if (!next_thread || next_thread->is_idle_thread) {
            if (!current || current_terminated || current_blocked) {
                next_thread = cpu->idle_thread;
            } else {
                cpu->ticks_remaining = cpu->quantum_ticks;
                __sync_lock_release(&cpu->scheduler_lock);
                return;
            }
        }


        // Re-queue current thread if it should continue (separate lock)
        if (current_should_continue) {
            current->state = THREAD_READY;
            current->next = nullptr;

            lock_ready_queue(cpu_id);
            if (cpu->ready_queue_tail) {
                cpu->ready_queue_tail->next = current;
                cpu->ready_queue_tail = current;
            } else {
                cpu->ready_queue_head = current;
                cpu->ready_queue_tail = current;
            }
            unlock_ready_queue(cpu_id);
        }

        // Switch to next thread
        if (next_thread && next_thread != current) {
            next_thread->state = THREAD_RUNNING;
            cpu->current_thread = next_thread;
            cpu->ticks_remaining = cpu->quantum_ticks;

            __sync_lock_release(&cpu->scheduler_lock);
            // Delegate actual context switch to thread_manager
            thread_manager::switch_to_thread(current, next_thread, frame);
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

    kthread_t *get_current_thread_on_cpu(uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        return cpu->current_thread;
    }

    bool is_cpu_enabled(uint8_t cpu_id) {
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);
        return cpu->scheduler_enabled;
    }

    void add_blocked_thread(kthread_t *thread, uint8_t cpu_id) {
        lock_blocked_queue(cpu_id);
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);

        thread->state = THREAD_BLOCKED;
        thread->next = cpu->blocked_queue_head;
        cpu->blocked_queue_head = thread;

        unlock_blocked_queue(cpu_id);
    }

    void wake_sleeping_threads(uint8_t cpu_id, uint64_t current_tick) {
        lock_blocked_queue(cpu_id);
        cpu_scheduler_t *cpu = get_cpu_data(cpu_id);

        kthread_t *prev = nullptr;
        kthread_t *thread = cpu->blocked_queue_head;

        while (thread) {
            if (current_tick >= thread->wakeup_tick) {
                // Remove from blocked queue
                kthread_t *to_wake = thread;
                if (prev) {
                    prev->next = thread->next;
                } else {
                    cpu->blocked_queue_head = thread->next;
                }
                thread = thread->next;

                to_wake->state = THREAD_READY;
                to_wake->next = nullptr;

                unlock_blocked_queue(cpu_id);

                // Add to ready queue (separate lock)
                lock_ready_queue(cpu_id);
                if (cpu->ready_queue_tail) {
                    cpu->ready_queue_tail->next = to_wake;
                    cpu->ready_queue_tail = to_wake;
                } else {
                    cpu->ready_queue_head = to_wake;
                    cpu->ready_queue_tail = to_wake;
                }
                unlock_ready_queue(cpu_id);

                cpu->need_resched = true;

                lock_blocked_queue(cpu_id);
                // Restart from beginning since we released the lock
                prev = nullptr;
                thread = cpu->blocked_queue_head;
            } else {
                prev = thread;
                thread = thread->next;
            }
        }

        unlock_blocked_queue(cpu_id);
    }
}
