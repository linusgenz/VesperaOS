//
// Created by Linus on 17.07.25.
//

#include "scheduler.h"
#include "spinlock.h"
#include "../cpu/cpu_manager.h"
#include "../../include/log.h"

static runqueue_t runqueues[MAX_CPU_CORES];
static kthread_t* current_threads[MAX_CPU_CORES];
static kthread_t* idle_threads[MAX_CPU_CORES];
static spinlock_t scheduler_locks[MAX_CPU_CORES];

extern "C" [[noreturn]] void idle_thread_func(void* arg) {
    while (true) {
        asm volatile("hlt");
    }
}

void scheduler_init_cpu(uint8_t cpu_id) {
    runqueues[cpu_id] = {};
    scheduler_locks[cpu_id].init();

    idle_threads[cpu_id] = create_kthread(idle_thread_func, &cpu_id, cpu_id);

    current_threads[cpu_id] = idle_threads[cpu_id];

    context_switch(nullptr, current_threads[cpu_id]);
}

void scheduler_add_thread(kthread_t* thread) {
    uint8_t cpu = thread->cpu_id;
    spinlock_guard lock(scheduler_locks[cpu]);

    thread->state = THREAD_READY;
    thread->next = nullptr;

    if (!runqueues[cpu].head) {
        runqueues[cpu].head = runqueues[cpu].tail = thread;
    } else {
        runqueues[cpu].tail->next = thread;
        runqueues[cpu].tail = thread;
    }
}

kthread_t* scheduler_pick_next(uint8_t cpu) {
    auto& rq = runqueues[cpu];

    if (!rq.head) return nullptr;

    kthread_t* next = rq.head;
    rq.head = rq.head->next;
    if (!rq.head) rq.tail = nullptr;

    next->next = nullptr;
    next->state = THREAD_RUNNING;
    return next;
}

void schedule(uint8_t cpu_id) {
    spinlock_guard lock(scheduler_locks[cpu_id]);

    kthread_t* current = current_threads[cpu_id];

    // If the current thread is allowed to continue, return to the back of the queue
    if (current && current->state == THREAD_RUNNING) {
        current->state = THREAD_READY;
        scheduler_add_thread(current);
    }

    kthread_t* next = scheduler_pick_next(cpu_id);

    if (!next) {
        next = idle_threads[cpu_id]; // no thread? go idle
    }

    current_threads[cpu_id] = next;

    if (current != next) {
        context_switch(current, next);
    }
}
