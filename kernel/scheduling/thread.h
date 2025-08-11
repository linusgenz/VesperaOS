//
// Created by Linus on 17.07.25.
//

#ifndef THREAD_H
#define THREAD_H
#include "stdint.h"

#define THREAD_KERNEL_STACK_SIZE (0x1000 * 2)

enum ThreadState {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
};

struct kthread_t {
    uint64_t id;
    ThreadState state;
    uint64_t wakeup_tick;
    void* stack;
    void* stack_pointer;
    void (*entry)(void*);
    void* arg;
    uint8_t cpu_id;
    bool is_user_thread;
    uint64_t saved_user_rsp;
    void* user_stack_top;
    void* user_entry;
    bool is_idle_thread;
    kthread_t* next;
};

struct sleeping_thread_t {
    kthread_t* thread;
    uint64_t wakeup_tick;
    sleeping_thread_t* next;
};

inline sleeping_thread_t* sleeping_list = nullptr;

kthread_t* create_kthread(void (*func)(void*), void* arg, uint8_t cpu_id);
kthread_t* create_idle_kthread(void (*func)(void*), uint8_t cpu_id);
kthread_t* create_user_thread(void* user_entry, void* user_stack_top);

#endif //THREAD_H
