//
// Created by Linus on 17.07.25.
//

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "stddef.h"
#include "thread.h"
#include "../acpi/madt.h"

#define SCHEDULER_QUANTUM_MS 5   // Time slice in milliseconds
#define SCHEDULER_TICK_MS    10  // Interrupt frequency

namespace kernel::scheduling {

    // Per-CPU scheduler data
    struct cpu_scheduler_t {
        kthread_t* ready_queue_head;
        kthread_t* ready_queue_tail;
        kthread_t* blocked_queue_head;
        kthread_t* current_thread;
        kthread_t* idle_thread;
        uint32_t quantum_ticks;
        uint32_t ticks_remaining;
        bool scheduler_enabled;
        uint32_t lock;  // Simple spinlock for SMP safety
    };

    struct scheduler_t {
        cpu_scheduler_t cpus[MAX_CPU_CORES];
        uint32_t num_cpus;
        bool initialized;
    };

    extern scheduler_t global_scheduler;

    // API
    void init(uint32_t num_cpus);
    void add_thread(kthread_t* thread);
    void remove_thread(kthread_t* thread);
    void yield();
    void tick();
    void enable_on_cpu(uint8_t cpu_id);
    void disable_on_cpu(uint8_t cpu_id);
    void cleanup_thread(kthread_t* thread);
    extern "C" void terminate_current_thread();
    kthread_t* get_current_thread();

    static inline void lock(uint32_t* lock) {
        while (__sync_lock_test_and_set(lock, 1)) {
            __asm__ volatile ("pause");
        }
    }

    static inline void unlock(uint32_t* lock) {
        __sync_lock_release(lock);
    }

} // namespace kernel::scheduling

extern "C" void context_switch(void** old_sp, void* new_sp);

#endif // SCHEDULER_H
