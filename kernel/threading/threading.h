// threading.h
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

#ifndef VESPERAOS_THREADING_H
#define VESPERAOS_THREADING_H

#include "cstdint"
#include "thread.h"
namespace kernel::threading {

    // Thread creation parameters
    struct ThreadCreateParams {
        const char* name;
        uint8_t priority;
        uint8_t cpu_id;
        uint64_t stack_size;
        void* custom_stack;  // nullptr for auto-allocation
        bool is_idle_thread;
        bool is_user_thread;
        kprocess_t* process;
    };

    class ThreadFactory {
    public:
        static void initialize();
        static bool is_initialized();

        static kthread_t* create_kernel_thread(const ThreadCreateParams& params,
                                             void(*entry_point)(void*), void* arg = nullptr);

        static kthread_t* create_user_thread(const ThreadCreateParams& params,
                                           void* user_entry, void* user_stack_top);

        static uint64_t allocate_thread_id();
        static void release_thread_id(uint64_t tid);

        static void* allocate_kernel_stack(uint64_t size = THREAD_STACK_SIZE);
        static void free_kernel_stack(void* stack, uint64_t size = THREAD_STACK_SIZE);

        static void cleanup_thread_resources(kthread_t* thread);

        static uint64_t get_total_threads_created();
        static uint64_t get_next_thread_id();

    private:
        static bool initialized;
        static uint64_t next_thread_id;
        static uint64_t total_threads_created;

        static kthread_t* allocate_thread_control_block();
        static void setup_kernel_thread_stack(kthread_t* thread, void(*entry_point)(void*), void* arg);
        static void setup_user_thread_stack(kthread_t* thread, void* user_entry, void* user_stack_top);
        static void setup_idle_thread_stack(kthread_t* thread, void(*entry_point)(void*));
    };

} // namespace kernel::threading


#endif //VESPERAOS_THREADING_H