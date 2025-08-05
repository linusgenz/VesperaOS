//
// Created by Linus on 17.07.25.
//

#include "thread.h"
#include "../include/scheduler.h"
#include "../../include/log.h"
#include <stddef.h>

#include "../cpu/cpu_manager.h"

static uint64_t next_thread_id = 1;

extern "C" void thread_trampoline();
extern "C" void usermode_entry_trampoline();

#define KERNEL_STACK_SIZE (4096 * 2)

// Helper: Allocate and map stack memory
static void* allocate_and_map_stack(size_t pages = 2) {
    void* stack = kernel::memory::request_pages(pages);
    if (!stack) return nullptr;

    for (size_t offset = 0; offset < pages * 0x1000; offset += 0x1000) {
        void* addr = (void*)((uintptr_t)stack + offset);
        kernel::memory::map_memory(addr, addr);
    }
    return stack;
}

// Internal base thread creation
static kthread_t* create_kthread_internal(void (*func)(void*), void* arg, uint8_t cpu_id, bool add_to_scheduler) {
    void* stack = allocate_and_map_stack();
    if (!stack) return nullptr;

    uintptr_t stack_top = (uintptr_t)stack + KERNEL_STACK_SIZE;
    uintptr_t* sp = reinterpret_cast<uintptr_t*>(stack_top);

    // Prepare initial kernel context
    *(--sp) = (uintptr_t)thread_trampoline; // Return RIP
    *(--sp) = 0x202;                        // RFLAGS
    *(--sp) = 0;                            // R15
    *(--sp) = 0;                            // R14
    *(--sp) = 0;                            // R13
    *(--sp) = 0;                            // R12
    *(--sp) = 0;                            // RBX
    *(--sp) = 0;                            // RBP

    // Thread control block
    kthread_t* thread = (kthread_t*)kernel::memory::request_page();
    kernel::memory::map_memory(thread, thread);

    thread->id = next_thread_id++;
    thread->state = THREAD_READY;
    thread->stack = stack;
    thread->stack_pointer = sp;
    thread->entry = func;
    thread->arg = arg;
    thread->cpu_id = cpu_id;
    thread->is_idle_thread = false;
    thread->is_user_thread = false;
    thread->user_entry = nullptr;
    thread->user_stack_top = nullptr;
    thread->next = nullptr;

    if (add_to_scheduler)
        kernel::scheduling::add_thread(thread);

    return thread;
}

// Wrapper: Create kernel thread
kthread_t* create_kthread(void (*func)(void*), void* arg, uint8_t cpu_id) {
    return create_kthread_internal(func, arg, cpu_id, true);
}

// Wrapper: Create idle thread (not added to scheduler)
kthread_t* create_idle_kthread(void (*func)(void*), uint8_t cpu_id) {
    auto t = create_kthread_internal(func, nullptr, cpu_id, false);
    t->is_idle_thread = true;
    return t;
}

void* prepare_usermode_trampoline(kthread_t* t) {
    uint8_t* stack = (uint8_t*) t->stack + KERNEL_STACK_SIZE;

    stack -= sizeof(void*); *(void**)stack = t->user_stack_top;   // RDI
    stack -= sizeof(void*); *(void**)stack = t->user_entry;       // RSI
    stack -= sizeof(void*); *(void**)stack = (void*)usermode_entry_trampoline;

    return stack;
}

kthread_t* create_user_thread(void* user_entry, void* user_stack_top) {
    // Create as dummy kernel thread with no func
    kthread_t* t = create_kthread_internal(nullptr, nullptr, CPUManager::get_current_cpu_id(), true);

    t->is_user_thread = true;
    t->user_entry = user_entry;
    t->user_stack_top = user_stack_top;

    // Overwrite kernel stack pointer to jump to user trampoline
    t->stack_pointer = prepare_usermode_trampoline(t);
    return t;
}
