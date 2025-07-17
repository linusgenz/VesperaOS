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

kthread_t* create_kthread(void (*func)(void*), void* arg, uint8_t cpu_id) {
    const size_t STACK_SIZE = 4096 * 2;

    void* stack = global_allocator.request_pages(2);
    if (!stack) return nullptr;
    for (size_t offset = 0; offset < STACK_SIZE; offset += 0x1000) {
        void* addr = (void*)((uintptr_t)stack + offset);
        global_page_table_manager.map_memory(addr, addr, false);
    }

    Log::PrintLn("entry(thread) %p", (uintptr_t)thread_trampoline);

    uintptr_t stack_top = (uintptr_t)stack + STACK_SIZE;

    uintptr_t* sp = (uintptr_t*)stack_top;

    *(--sp) = (uintptr_t)thread_trampoline;
    *(--sp) = (uintptr_t)arg;   // rdi
    *(--sp) = (uintptr_t)func;  // rax

    *(--sp) = 0; // r15
    *(--sp) = 0; // r14
    *(--sp) = 0; // r13
    *(--sp) = 0; // r12
    *(--sp) = 0; // rbx
    *(--sp) = 0; // rbp

    kthread_t* thread = (kthread_t*)global_allocator.request_page();
    global_page_table_manager.map_memory(thread, thread, false);
    thread->id = next_thread_id++;
    thread->state = THREAD_READY;
    thread->stack = stack;
    thread->stack_pointer = (void*)sp;
    thread->entry = func;
    thread->arg = arg;
    thread->cpu_id = cpu_id;
    thread->next = nullptr;

    scheduler_add_thread(thread);

    return thread;
}
