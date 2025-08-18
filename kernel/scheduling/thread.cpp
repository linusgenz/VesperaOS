//
// Created by Linus on 17.07.25.
//

#include "thread.h"
#include <scheduling.h>
#include "../../include/log.h"
#include <stddef.h>

#include "../cpu/cpu_manager.h"

static uint64_t next_thread_id = 1;

extern "C" void thread_trampoline();

static void* allocate_and_map_stack(size_t pages = 2) {
    void* stack = kernel::memory::request_pages(pages);
    if (!stack) return nullptr;

    for (size_t offset = 0; offset < pages * 0x1000; offset += 0x1000) {
        void* addr = (void*)((uintptr_t)stack + offset);
        kernel::memory::map_memory(addr, addr);
    }
    return stack;
}

static kthread_t* create_kthread_internal(void (*func)(void*), void* arg, uint8_t cpu_id) {
    void* stack = allocate_and_map_stack();
    if (!stack) return nullptr;

    uintptr_t stack_top = (uintptr_t)stack + THREAD_KERNEL_STACK_SIZE;
    uintptr_t* sp = reinterpret_cast<uintptr_t*>(stack_top);

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
    thread->from_syscall = false;
    thread->next = nullptr;

    return thread;
}

kthread_t* create_kthread(void (*func)(void*), void* arg, uint8_t cpu_id) {
    auto t = create_kthread_internal(func, arg, cpu_id);
    auto* sp = (uintptr_t*)((uint8_t*)t->stack_pointer);
    *(--sp) = (uintptr_t)thread_trampoline; // Return RIP
    *(--sp) = 0x202;                        // RFLAGS
    *(--sp) = 0;                            // R15
    *(--sp) = 0;                            // R14
    *(--sp) = 0;                            // R13
    *(--sp) = 0;                            // R12
    *(--sp) = 0;                            // RBX
    *(--sp) = 0;                            // RBP

    t->stack_pointer = sp;
    return t;
}

kthread_t* create_idle_kthread(void (*func)(void*), uint8_t cpu_id) {
    auto t = create_kthread_internal(func, nullptr, cpu_id);

    auto* sp = (uintptr_t*)((uint8_t*)t->stack_pointer);
    *(--sp) = (uintptr_t)func;              // Return RIP
    *(--sp) = 0x202;                        // RFLAGS
    *(--sp) = 0;                            // R15
    *(--sp) = 0;                            // R14
    *(--sp) = 0;                            // R13
    *(--sp) = 0;                            // R12
    *(--sp) = 0;                            // RBX
    *(--sp) = 0;                            // RBP

    t->stack_pointer = sp;
    t->is_idle_thread = true;
    return t;
}

kthread_t* create_user_thread(void* user_entry, void* user_stack_top) {
    kthread_t* t = create_kthread_internal(nullptr, nullptr, CPUManager::get_current_cpu_id());

    auto* sp = (uintptr_t*)((uint8_t*)t->stack_pointer);

    // IRETQ-Frame
    *(--sp) = 0x23;                  // SS
    *(--sp) = (uintptr_t)user_stack_top; // RSP_user
    *(--sp) = 0x202;                  // RFLAGS
    *(--sp) = 0x1B;                  // CS
    *(--sp) = (uintptr_t)user_entry; // RIP

  //  *(--sp) = 0;                            // Return RIP
  /*  *(--sp) = 0x202;                        // RFLAGS
    *(--sp) = 0;                            // R15
    *(--sp) = 0;                            // R14
    *(--sp) = 0;                            // R13
    *(--sp) = 0;                            // R12
    *(--sp) = 0;                            // RBX
    *(--sp) = 0;              */              // RBP


    t->stack_pointer = sp;
    t->is_user_thread = true;
    t->user_entry = user_entry;
    t->user_stack_top = user_stack_top;

    return t;
}