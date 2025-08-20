//
// Created by Linus on 17.07.25.
//
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <cstdint>
#include "../scheduling/cpu_scheduler.h"

namespace kernel::scheduling {

    struct global_scheduler_t {
        cpu_scheduler::cpu_scheduler_t cpus[MAX_CPU_CORES];
        uint32_t num_cpus;
        bool initialized;
    };

    extern global_scheduler_t global_scheduler;
    
    // Global scheduler operations
    void init(uint32_t num_cpus);
    void yield();
 //   void tick();

    // Global thread management
    void add_thread(kthread_t* thread);
    void remove_thread(kthread_t* thread);
    void thread_exit();

    // CPU management
    void enable_on_cpu(uint8_t cpu_id);
    void disable_on_cpu(uint8_t cpu_id);

    // Query functions
    kthread_t* get_current_thread();
    bool is_initialized();
    uint32_t get_num_cpus();

    kthread_t* create_kthread(void (*func)(void*), void* arg, uint8_t cpu_id);

} // namespace kernel::scheduling::scheduler

#endif // SCHEDULER_H