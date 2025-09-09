// process_manager.h
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

#ifndef VESPERAOS_PROCESS_MANAGER_H
#define VESPERAOS_PROCESS_MANAGER_H

#include <cstdint>
#include "process.h"
#include "../threading/thread.h"

namespace kernel::process {

    struct ProcessStatistics {
        uint64_t total_processes_created;
        uint64_t total_threads_created;
        uint64_t active_processes; // 1
        uint64_t active_threads; // 2
        uint64_t kernel_processes;
        uint64_t user_processes;
        uint64_t zombie_processes;
        uint64_t running_threads;
        uint64_t ready_threads;
        uint64_t blocked_threads;
        uint64_t terminated_threads;
        uint64_t memory_used_by_processes;  // in bytes
        uint64_t cpu_time_total;            // in ticks
    };

    // Process creation options
    struct ProcessCreateOptions {
        const char* name;
        uint8_t priority;
        uint8_t cpu_id;
        uint64_t heap_start;
        uint64_t heap_size;
        uint64_t stack_size;
        bool is_kernel_process;
        void* custom_pml4;  // nullptr for auto-creation
    };

    struct ProcessInfo {
        uint64_t pid;
        char name[64];
        ProcessState state;
        uint64_t thread_count;
        uint64_t memory_usage;
        uint64_t cpu_time;
        uint64_t creation_time;
        bool is_kernel_process;
    };

    struct ThreadInfo {
        uint64_t tid;
        uint64_t pid;
        ThreadState state;
        uint8_t cpu_id;
        uint8_t priority;
        uint64_t cpu_time;
        uint64_t stack_usage;
    };

    class ProcessIterator {
    private:
        kprocess_t* current_process;

    public:
        ProcessIterator(kprocess_t* start = nullptr);
        ProcessIterator& operator++();
        ProcessInfo operator*() const;
        bool operator!=(const ProcessIterator& other) const;
        kprocess_t* get_raw_process() const { return current_process; }
    };

    class ThreadIterator {
    private:
        kprocess_t* current_process;
        kthread_t* current_thread;

    public:
        ThreadIterator(kprocess_t* proc = nullptr, kthread_t* thread = nullptr);
        ThreadIterator& operator++();
        ThreadInfo operator*() const;
        bool operator!=(const ThreadIterator& other) const;
        kthread_t* get_raw_thread() const { return current_thread; }
    };

    class Manager {
    public:
        static void initialize();
        static bool is_initialized();

        static kprocess_t* create_process(const ProcessCreateOptions& options,
                                        void* entry_point, void* user_stack_top = nullptr);
        static kprocess_t* create_process_from_elf(const ProcessCreateOptions& options,
                                                 const char* elf_path);
        static kprocess_t* create_kernel_process(const ProcessCreateOptions& options,
                                               void(*entry_point)(void*), void* arg = nullptr);

        static bool terminate_process(uint64_t pid, int exit_code = 0);
        static bool kill_process(uint64_t pid);

        static void cleanup_process(kprocess_t *proc);

        static bool suspend_process(uint64_t pid);
        static bool resume_process(uint64_t pid);

        static kprocess_t* find_process(uint64_t pid);
        static kprocess_t* find_process_by_name(const char* name);
        static kprocess_t* get_current_process();

        static kthread_t* create_thread_in_process(uint64_t pid, void* entry_point,
                                                 void* arg = nullptr, uint8_t priority = 5);
        static bool terminate_thread(uint64_t tid);
        static bool suspend_thread(uint64_t tid);
        static bool resume_thread(uint64_t tid);

        static kthread_t* find_thread(uint64_t tid);
        static kthread_t* get_current_thread();

        static ProcessStatistics get_statistics();
        static void update_statistics();
        static void reset_statistics();

        static ProcessIterator begin_processes();
        static ProcessIterator end_processes();
        static ThreadIterator begin_threads();
        static ThreadIterator end_threads();
        static ThreadIterator begin_process_threads(uint64_t pid);
        static ThreadIterator end_process_threads(uint64_t pid);

        static uint64_t get_process_memory_usage(uint64_t pid);
        static bool expand_process_heap(uint64_t pid, uint64_t size);
        static bool shrink_process_heap(uint64_t pid, uint64_t size);

        static void cleanup_zombie_processes();
        static void cleanup_terminated_threads();

        static bool all_threads_from_proc_terminated(kprocess_t* proc);

    private:
        static bool initialized;
        static ProcessStatistics stats;
        static uint64_t next_pid;
        static kprocess_t* process_list_head;

        static void add_process_to_list(kprocess_t* proc);
        static void remove_process_from_list(kprocess_t* proc);

        void free_process_user_pages(kprocess_t *proc);

        static void cleanup_process_internal(kprocess_t* proc);
        static void update_process_statistics(kprocess_t* proc, bool increment);
        static void update_thread_statistics(kthread_t* thread, bool increment);
    };

    namespace util {
        ProcessStatistics get_system_stats();
        void print_process_list();
        void print_thread_list();
        void print_process_tree();

        double get_cpu_usage_percent();
        uint64_t get_total_memory_usage();
        uint32_t get_load_average();
    }

} // namespace kernel::process

#define PROCESS_MANAGER kernel::process::Manager
#define CURRENT_PROCESS() kernel::process::Manager::get_current_process()
#define CURRENT_THREAD() kernel::process::Manager::get_current_thread()

#endif //VESPERAOS_PROCESS_MANAGER_H