//
// Created by Linus on 17.07.25.
//

#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "stddef.h"
#include "thread.h"
constexpr size_t MAX_THREADS = 256;

struct runqueue_t {
    kthread_t* head;
    kthread_t* tail;
};

void scheduler_init_cpu(uint8_t cpu_id);
void scheduler_add_thread(kthread_t* thread);
void schedule(uint8_t cpu_id);

extern "C" void context_switch(kthread_t* old_thread, kthread_t* new_thread);

#endif //SCHEDULER_H
