// threading.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 08.09.25.
//
// This file is part of VesperaOS.
// 
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include "threading.h"
#include <log.h>
#include <string.h>

#include "cstdint"
#include "../cpu/cpu_manager.h"
#include "../include/time.h"

extern "C" void thread_trampoline();

namespace kernel::threading {
    // Static member definitions
    bool ThreadFactory::initialized = false;
    uint64_t ThreadFactory::next_thread_id = 1;
    uint64_t ThreadFactory::total_threads_created = 0;

    void ThreadFactory::initialize() {
        if (initialized) return;

        next_thread_id = 1;
        total_threads_created = 0;
        initialized = true;
    }

    bool ThreadFactory::is_initialized() {
        return initialized;
    }

    uint64_t ThreadFactory::allocate_thread_id() {
        if (!initialized) return 0;
        return next_thread_id++;
    }

    void ThreadFactory::release_thread_id(uint64_t tid) {
        // For future implementation of TID recycling
        // For now, we don't recycle TIDs to avoid conflicts
    }

    void *ThreadFactory::allocate_kernel_stack(uint64_t size) {
        if (!size) size = THREAD_STACK_SIZE;

        size_t pages = (size + 0xFFF) / 0x1000; // Round up to pages
        void *stack = kernel::memory::request_pages(pages);
        if (!stack) return nullptr;

        // Map stack pages
        for (size_t offset = 0; offset < pages * 0x1000; offset += 0x1000) {
            void *addr = (void *) ((uintptr_t) stack + offset);
            kernel::memory::map_memory(addr, addr);
        }

        return stack;
    }

    void ThreadFactory::free_kernel_stack(void *stack, uint64_t size) {
        if (!stack) return;

        if (!size) size = THREAD_STACK_SIZE;
        size_t pages = (size + 0xFFF) / 0x1000;

        // Unmap pages
        for (size_t offset = 0; offset < pages * 0x1000; offset += 0x1000) {
            void *addr = (void *) ((uintptr_t) stack + offset);
            kernel::memory::unmap_memory(addr);
        }

        kernel::memory::free_pages(stack, pages);
    }

    kthread_t *ThreadFactory::allocate_thread_control_block() {
        auto *thread = (kthread_t *) kernel::memory::request_page();
        if (!thread) return nullptr;

        kernel::memory::map_memory(thread, thread);
        memset(thread, 0, sizeof(kthread_t));

        return thread;
    }

    kthread_t *ThreadFactory::create_kernel_thread(const ThreadCreateParams &params,
                                                   void (*entry_point)(void *), void *arg) {
        if (!initialized || !entry_point) return nullptr;

        kthread_t *thread = allocate_thread_control_block();
        if (!thread) return nullptr;

        // Allocate stack
        void *stack = params.custom_stack ? params.custom_stack : allocate_kernel_stack(params.stack_size);
        if (!stack) {
            kernel::memory::free_page(thread);
            return nullptr;
        }

        // Initialize thread control block
        thread->tid = allocate_thread_id();
        thread->state = THREAD_READY;
        thread->stack = stack;
        thread->stack_size = params.stack_size ? params.stack_size : THREAD_STACK_SIZE;
        thread->stack_top = (void *) ((uintptr_t) stack + thread->stack_size);
        thread->stack = stack;
        thread->stack_pointer = thread->stack_top;

        thread->entry = entry_point;
        thread->arg = arg;
        thread->cpu_id = params.cpu_id;
        thread->priority = params.priority;
        thread->is_idle_thread = params.is_idle_thread;
        thread->is_user_thread = false;
        thread->user_entry = nullptr;
        thread->user_stack_top = nullptr;
        thread->from_syscall = false;
        thread->next = nullptr;
        thread->prev = nullptr;
        thread->process = params.process;

        thread->creation_time = kernel::time::get_uptime_ms();

        if (params.name) {
            strncpy(thread->name, params.name, sizeof(thread->name) - 1);
            thread->name[sizeof(thread->name) - 1] = '\0';
        } else {
            snprintf(thread->name, sizeof(thread->name),
                     "kernel_thread %llu", thread->tid);
        }

        // Setup stack for kernel thread
        setup_kernel_thread_stack(thread, entry_point, arg);

        total_threads_created++;

         //  Log::debug("Created kernel thread TID=%llu (%s) on CPU %d",
         //           thread->tid, thread->name ? thread->name : "unnamed", params.cpu_id);

        return thread;
    }

    kthread_t *ThreadFactory::create_user_thread(const ThreadCreateParams &params,
                                                 void *user_entry, void *user_stack_top) {
        if (!initialized || !user_entry) return nullptr;

        kthread_t *thread = allocate_thread_control_block();
        if (!thread) return nullptr;

        // Allocate kernel stack for user thread
        void *kernel_stack = allocate_kernel_stack(params.stack_size);
        if (!kernel_stack) {
            kernel::memory::free_page(thread);
            return nullptr;
        }

        // Initialize thread control block
        thread->tid = allocate_thread_id();
        thread->state = THREAD_READY;
        thread->stack = kernel_stack;
        thread->stack_size = params.stack_size ? params.stack_size : THREAD_STACK_SIZE;
        thread->stack_top = (void *) ((uintptr_t) kernel_stack + thread->stack_size);
        thread->stack = kernel_stack;
        thread->stack_pointer = thread->stack_top;

        thread->entry = nullptr; // No kernel entry for user threads
        thread->arg = nullptr;
        thread->cpu_id = params.cpu_id;
        thread->priority = params.priority;
        thread->is_idle_thread = false;
        thread->is_user_thread = true;
        thread->user_entry = user_entry;
        thread->user_stack_top = user_stack_top;
        thread->from_syscall = false;
        thread->next = nullptr;
        thread->prev = nullptr;
        thread->process = params.process;

        thread->creation_time = kernel::time::get_uptime_ms();

        if (params.name) {
            strncpy(thread->name, params.name, sizeof(thread->name) - 1);
        } else {
            const auto name = "unnamed";
            strncpy(thread->name, name, strlen(name));
        }

        // Setup stack for user thread
        setup_user_thread_stack(thread, user_entry, user_stack_top);

        total_threads_created++;

        Log::debug("Created user thread TID=%llu (%s) on CPU %d",
                   thread->tid, params.name ? params.name : "unnamed", params.cpu_id);

        return thread;
    }

    void ThreadFactory::setup_kernel_thread_stack(kthread_t *thread, void (*entry_point)(void *), void *arg) {
        uintptr_t *sp = (uintptr_t *) thread->stack_pointer;

        // Setup stack for context switching
        *(--sp) = (uintptr_t) thread_trampoline; // Return RIP
        *(--sp) = 0x202; // RFLAGS
        *(--sp) = 0; // R15
        *(--sp) = 0; // R14
        *(--sp) = 0; // R13
        *(--sp) = 0; // R12
        *(--sp) = 0; // RBX
        *(--sp) = 0; // RBP

        thread->stack_pointer = sp;
    }

    void ThreadFactory::setup_user_thread_stack(kthread_t *thread, void *user_entry, void *user_stack_top) {
        uintptr_t *sp = (uintptr_t *) thread->stack_pointer;

        // Setup IRETQ frame for user mode
        *(--sp) = 0x23; // SS (user data selector)
        *(--sp) = (uintptr_t) user_stack_top; // RSP_user
        *(--sp) = 0x202; // RFLAGS
        *(--sp) = 0x1B; // CS (user code selector)
        *(--sp) = (uintptr_t) user_entry; // RIP

        thread->stack_pointer = sp;
    }

    void ThreadFactory::setup_idle_thread_stack(kthread_t *thread, void (*entry_point)(void *)) {
        uintptr_t *sp = (uintptr_t *) thread->stack_pointer;

        // Idle threads jump directly to their function
        *(--sp) = (uintptr_t) entry_point; // Return RIP
        *(--sp) = 0x202; // RFLAGS
        *(--sp) = 0; // R15
        *(--sp) = 0; // R14
        *(--sp) = 0; // R13
        *(--sp) = 0; // R12
        *(--sp) = 0; // RBX
        *(--sp) = 0; // RBP

        thread->stack_pointer = sp;
    }

    void ThreadFactory::cleanup_thread_resources(kthread_t *thread) {
        if (!thread) return;

        Log::debug("Cleaning up thread TID=%llu resources", thread->tid);

        // Free kernel stack
        if (thread->stack && !thread->is_user_thread) {
            free_kernel_stack(thread->stack, thread->stack_size);
        }

        // Free thread control block
        kernel::memory::free_page(thread);
    }

    uint64_t ThreadFactory::get_total_threads_created() {
        return total_threads_created;
    }

    uint64_t ThreadFactory::get_next_thread_id() {
        return next_thread_id;
    }
} // namespace kernel::threading
