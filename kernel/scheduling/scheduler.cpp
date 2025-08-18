//
// Created by Linus on 17.07.25.
//

#include <scheduling.h>

#include <log.h>

#include "cpu_scheduler.h"
#include "thread_manager.h"
#include "../cpu/cpu_manager.h"
#include "thread.h"

namespace kernel::scheduling {

    global_scheduler_t global_scheduler = {{}};

    void init(uint32_t num_cpus) {
        global_scheduler.num_cpus = num_cpus;
        global_scheduler.initialized = true;

        for (uint32_t i = 0; i < num_cpus; i++) {
            cpu_scheduler::init_cpu(i);
        }
    }

    kthread_t* create_kthread(void (*func)(void*), void* arg, uint8_t cpu_id) {
        return ::create_kthread(func, arg, cpu_id);
    }

    void add_thread(kthread_t* thread) {
        if (!thread || !global_scheduler.initialized) return;

        uint8_t cpu_id = thread->cpu_id;
        if (cpu_id >= global_scheduler.num_cpus) return;

        thread_manager::add_thread(thread);
    }

    void thread_exit() {
        if (!global_scheduler.initialized) return;

        kthread_t* thread = get_current_thread();
        remove_thread(thread);
        thread_manager::cleanup_thread(thread);

    }

    void remove_thread(kthread_t* thread) {
        if (!thread || !global_scheduler.initialized) return;

        uint8_t cpu_id = thread->cpu_id;
        if (cpu_id >= global_scheduler.num_cpus) return;

        thread_manager::remove_thread(thread);
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

    kthread_t* get_current_thread() {
        return thread_manager::get_current_thread();
    }

    bool is_initialized() {
        return global_scheduler.initialized;
    }

    uint32_t get_num_cpus() {
        return global_scheduler.num_cpus;
    }

} // namespace kernel::scheduling::scheduler