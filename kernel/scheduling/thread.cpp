//
// Created by Linus on 17.07.25.
//

#include "thread.h"
#include "scheduler.h"
#include "../../include/log.h"
#include <stddef.h>

#include "../include/page_frame_allocator.h"
#include "../include/page_table_manager.h"

static uint64_t next_thread_id = 1;

extern "C" void thread_trampoline();

void dump_stack(uintptr_t* sp) {
    Log::PrintLn("Stack dump from %p:", sp);
    for (int i = 0; i < 17; i++) {
        Log::PrintLn("[%2d] = 0x%016lx", i, sp[i]);
    }
}


static kthread_t* create_kthread_internal(void (*func)(void*), void* arg, uint8_t cpu_id, bool add_to_scheduler) {
    const size_t STACK_SIZE = 4096 * 2;

    void* stack = global_allocator.request_pages(2);
    if (!stack) return nullptr;

    for (size_t offset = 0; offset < STACK_SIZE; offset += 0x1000) {
        void* addr = (void*)((uintptr_t)stack + offset);
        global_page_table_manager.map_memory(addr, addr, false);
    }

    uintptr_t stack_top = (uintptr_t)stack + STACK_SIZE;
    uintptr_t* sp = (uintptr_t*)stack_top;

    *(--sp) = (uintptr_t)thread_trampoline;  // Return address
    *(--sp) = 0x202;         // RFLAGS
    *(--sp) = 0;             // R15
    *(--sp) = 0;             // R14
    *(--sp) = 0;             // R13
    *(--sp) = 0;             // R12
    *(--sp) = 0;             // RBX
    *(--sp) = 0;             // RBP

    auto thread = (kthread_t*)global_allocator.request_page();
    global_page_table_manager.map_memory(thread, thread, false);
    thread->id = next_thread_id++;
    thread->state = THREAD_READY;
    thread->stack = stack;
    thread->stack_pointer = (void*)sp;
    thread->entry = func;
    thread->arg = arg;
    thread->cpu_id = cpu_id;
    thread->is_idle_thread = false;
    thread->next = nullptr;

    if (add_to_scheduler) {
        kernel::scheduling::add_thread(thread);
    }

    return thread;
}

kthread_t* create_kthread(void (*func)(void*), void* arg, uint8_t cpu_id) {
    return create_kthread_internal(func, arg, cpu_id, true);
}

kthread_t* create_idle_kthread(void (*func)(void*), uint8_t cpu_id) {
    auto t = create_kthread_internal(func, nullptr, cpu_id, false);
    t->is_idle_thread = true;
    return t;
}
