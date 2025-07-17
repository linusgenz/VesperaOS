//
// Created by Linus on 17.07.25.
//

#ifndef THREAD_H
#define THREAD_H
#include "stdint.h"

enum ThreadState {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
};

struct kthread_t {
    uint64_t id;
    ThreadState state;
    void* stack;
    void* stack_pointer;
    void (*entry)(void*);
    void* arg;
    uint8_t cpu_id;
    kthread_t* next;
};

kthread_t* create_kthread(void (*func)(void*), void* arg, uint8_t cpu_id);

#endif //THREAD_H
