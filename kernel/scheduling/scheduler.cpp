//
// Created by Linus on 17.07.25.
//

#include "scheduler.h"
#include "spinlock.h"
#include "../../include/log.h"
#include "../cpu/cpu_manager.h"
#include "../include/page_frame_allocator.h"

namespace kernel::scheduling {
    scheduler_t global_scheduler = {nullptr};

    extern "C" [[noreturn]] void idle_thread_func(void *arg) {
        uint32_t cpu_id = CPUManager::get_current_cpu_id();

        while (true) {
            __asm__ volatile ("pause");

            cpu_scheduler_t *cpu = &global_scheduler.cpus[cpu_id];
            if (cpu->ready_queue_head) {
                yield();
            }
        }
    }

    void init(uint32_t num_cpus) {
        global_scheduler.num_cpus = num_cpus;
        global_scheduler.initialized = true;

        for (uint32_t i = 0; i < num_cpus; i++) {
            cpu_scheduler_t *cpu = &global_scheduler.cpus[i];
            cpu->ready_queue_head = nullptr;
            cpu->ready_queue_tail = nullptr;
            cpu->current_thread = nullptr;
            cpu->idle_thread = nullptr;
            cpu->quantum_ticks = SCHEDULER_QUANTUM_MS / SCHEDULER_TICK_MS;
            cpu->ticks_remaining = cpu->quantum_ticks;
            cpu->scheduler_enabled = false;
            cpu->lock = 0;

            cpu->idle_thread = create_idle_kthread(idle_thread_func, i);
        }
    }

    void add_thread(kthread_t *thread) {
        if (!thread || !global_scheduler.initialized) return;

        uint8_t cpu_id = thread->cpu_id;
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler_t *cpu = &global_scheduler.cpus[cpu_id];
        lock(&cpu->lock);

        thread->state = THREAD_READY;
        thread->next = nullptr;

        if (cpu->ready_queue_tail) {
            cpu->ready_queue_tail->next = thread;
            cpu->ready_queue_tail = thread;
        } else {
            cpu->ready_queue_head = thread;
            cpu->ready_queue_tail = thread;
        }

        unlock(&cpu->lock);
    }

    void remove_thread(kthread_t *thread) {
        if (!thread || !global_scheduler.initialized) return;

        uint8_t cpu_id = thread->cpu_id;
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler_t *cpu = &global_scheduler.cpus[cpu_id];
        lock(&cpu->lock);

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

        unlock(&cpu->lock);
    }

    void yield() {
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        cpu_scheduler_t *cpu = &global_scheduler.cpus[cpu_id];
        if (!cpu->scheduler_enabled) return;

        lock(&cpu->lock);

        kthread_t *current = cpu->current_thread;
        kthread_t *next_thread = nullptr;


        if (cpu->ready_queue_head) {
            next_thread = cpu->ready_queue_head;
            cpu->ready_queue_head = next_thread->next;
            if (cpu->ready_queue_tail == next_thread) {
                cpu->ready_queue_tail = nullptr;
            }
            next_thread->next = nullptr;
        } else {
            next_thread = cpu->idle_thread;
        }

        bool current_terminated = (current && current->state == THREAD_TERMINATED);
        bool current_blocked = (current && current->state == THREAD_BLOCKED);
        bool current_should_continue = (current && !current_terminated && !current_blocked && !current->is_idle_thread
                                        && current->state == THREAD_RUNNING);

        // if current thread is the only thread on the core
        if (!next_thread || next_thread->is_idle_thread) {
            if (!current || current_terminated || current_blocked) {
                next_thread = cpu->idle_thread;
            } else {
                cpu->ticks_remaining = cpu->quantum_ticks;
                unlock(&cpu->lock);
                return;
            }
        }


        if (current_should_continue) {
            current->state = THREAD_READY;
            current->next = nullptr;
            if (cpu->ready_queue_tail) {
                cpu->ready_queue_tail->next = current;
                cpu->ready_queue_tail = current;
            } else {
                cpu->ready_queue_head = current;
                cpu->ready_queue_tail = current;
            }
        }


        if (next_thread && next_thread != current) {
            next_thread->state = THREAD_RUNNING;
            cpu->current_thread = next_thread;
            cpu->ticks_remaining = cpu->quantum_ticks;

            unlock(&cpu->lock);

            if (current != nullptr && !current_terminated) {
                auto ptr1 = &current->stack_pointer;

                context_switch(ptr1, next_thread->stack_pointer);
            } else {
                cleanup_thread(current);

                context_switch(nullptr, next_thread->stack_pointer);
            }
        } else {
            unlock(&cpu->lock);
        }
    }

    void cleanup_thread(kthread_t *thread) {
        if (!thread || thread->is_idle_thread) return;

        global_allocator.free_pages(thread->stack, 2);
        global_allocator.free_page(thread);
    }

    kthread_t *get_current_thread() {
        uint32_t cpu_id = CPUManager::get_current_cpu_id();
        cpu_scheduler_t *cpu = &global_scheduler.cpus[cpu_id];
        return cpu->current_thread;
    }

    void tick() {
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        cpu_scheduler_t *cpu = &global_scheduler.cpus[cpu_id];

        if (!cpu->scheduler_enabled) return;

        lock(&cpu->lock);

        if (cpu->ticks_remaining > 0) {
            cpu->ticks_remaining--;
        }

        bool should_yield = (cpu->ticks_remaining == 0);
        unlock(&cpu->lock);

        if (should_yield) {
            yield();
        }
    }

    void enable_on_cpu(uint8_t cpu_id) {
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler_t *cpu = &global_scheduler.cpus[cpu_id];
        cpu->scheduler_enabled = true;
        cpu->ticks_remaining = cpu->quantum_ticks;
    }

    void disable_on_cpu(uint8_t cpu_id) {
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler_t *cpu = &global_scheduler.cpus[cpu_id];
        cpu->scheduler_enabled = false;
    }

    extern "C" void terminate_current_thread() {
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        cpu_scheduler_t *cpu = &global_scheduler.cpus[cpu_id];

        if (!cpu->current_thread || cpu->current_thread->is_idle_thread) return;

        lock(&cpu->lock);
        cpu->current_thread->state = THREAD_TERMINATED;
        unlock(&cpu->lock);

        yield();

        while (true) {
            asm volatile("hlt");
        }
    }

    extern "C" void thread_trampoline() {
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        kthread_t *current = global_scheduler.cpus[cpu_id].current_thread;

        asm volatile("sti");
        current->entry(&current->id);
        Log::LogMsg("exiting thread %u", current->id);
        terminate_current_thread();
    }
} // namespace kernel::scheduling
