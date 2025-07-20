//
// Created by Linus on 17.07.25.
//
#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

#include "thread.h"
#include "stdint.h"

extern "C" void context_switch(void** old_sp, void* new_sp);

namespace kernel::scheduling::thread_manager {

    // Thread lifecycle management
    void add_thread(kthread_t* thread);
    void remove_thread(kthread_t* thread);
    void cleanup_thread(kthread_t* thread);

    // Thread state management
    [[noreturn]] void terminate_current_thread();
    kthread_t* get_current_thread();

    // Thread execution helpers
    extern "C" void thread_trampoline();
    extern "C" [[noreturn]] void idle_thread_func(void* arg);

    // Internal thread operations
    void switch_to_thread(kthread_t* from, kthread_t* to);
    void setup_idle_thread(uint8_t cpu_id);

} // namespace kernel::scheduling::thread_manager

#endif // THREAD_MANAGER_H