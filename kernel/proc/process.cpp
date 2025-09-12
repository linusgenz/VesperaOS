// process.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 11.08.25.
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


#include <scheduling.h>
#include "../../include/string.h"
#include "../exec/elf.h"
#include "../scheduling/thread_manager.h"
#include "process.h"
#include "process_manager.h"
#include <log.h>

#include "../cpu/cpu_manager.h"
#include "../threading/threading.h"
#include "../include/time.h"
#include "process_memory_manager.h"

namespace kernel::process {
    // Static member definitions
    bool Manager::initialized = false;
    ProcessStatistics Manager::stats = {};
    uint64_t Manager::next_pid = 1;
    kprocess_t *Manager::process_list_head = nullptr;

    // ProcessIterator implementation
    ProcessIterator::ProcessIterator(kprocess_t *start) : current_process(start) {
    }

    ProcessIterator &ProcessIterator::operator++() {
        if (current_process) {
            current_process = current_process->next;
        }
        return *this;
    }

    ProcessInfo ProcessIterator::operator*() const {
        ProcessInfo info = {};
        if (current_process) {
            info.pid = current_process->pid;
            strncpy(info.name, current_process->name, sizeof(info.name) - 1);
            info.state = current_process->state;
            info.is_kernel_process = (current_process->pml4 ==
                                      (PageTable *) kernel::memory::get_pagetable_address());

            kthread_t *t = current_process->thread_list;
            while (t) {
                info.thread_count++;
                t = t->next_in_process;
            }

            info.memory_usage = Manager::get_process_memory_usage(current_process->pid);
            info.cpu_time = current_process->cpu_time;
            info.creation_time = current_process->creation_time;
        }
        return info;
    }

    bool ProcessIterator::operator!=(const ProcessIterator &other) const {
        return current_process != other.current_process;
    }

    // ThreadIterator implementation
    ThreadIterator::ThreadIterator(kprocess_t *proc, kthread_t *thread)
        : current_process(proc), current_thread(thread) {
        if (!current_thread && current_process) {
            current_thread = current_process->thread_list;
        }
    }

    ThreadIterator &ThreadIterator::operator++() {
        if (current_thread) {
            current_thread = current_thread->next_in_process;
            if (!current_thread && current_process) {
                current_process = current_process->next;
                if (current_process) {
                    current_thread = current_process->thread_list;
                }
            }
        }
        return *this;
    }

    ThreadInfo ThreadIterator::operator*() const {
        ThreadInfo info = {};
        if (current_thread) {
            info.tid = current_thread->tid;
            info.pid = current_thread->process->pid;
            info.state = current_thread->state;
            info.cpu_id = current_thread->cpu_id;
            info.priority = current_thread->priority;
            info.stack_usage = current_thread->stack_size;
        }
        return info;
    }

    bool ThreadIterator::operator!=(const ThreadIterator &other) const {
        return current_thread != other.current_thread ||
               current_process != other.current_process;
    }

    void Manager::initialize() {
        if (initialized) return;

        threading::ThreadFactory::initialize();

        memset(&stats, 0, sizeof(ProcessStatistics));
        next_pid = 1;
        process_list_head = nullptr;
        initialized = true;
    }

    bool Manager::is_initialized() {
        return initialized;
    }

    kprocess_t *Manager::create_process(const ProcessCreateOptions &options,
                                        void *entry_point, void *user_stack_top) {
        if (!initialized) return nullptr;

        auto *proc = (kprocess_t *) memory::request_page();
        if (!proc) return nullptr;

        memset(proc, 0, sizeof(kprocess_t));

        proc->pid = next_pid++;
        strncpy(proc->name, options.name, sizeof(proc->name) - 1);
        proc->state = PROCESS_READY;
        proc->creation_time = kernel::time::get_uptime_ms();
        proc->is_kernel_process = options.is_kernel_process;

        // Create page table
        if (options.custom_pml4) {
            proc->pml4 = (PageTable *) options.custom_pml4;
        } else if (options.is_kernel_process) {
            proc->pml4 = (PageTable *) kernel::memory::get_pagetable_address();
        } else {
            proc->pml4 = kernel::memory::create_user_pagetable();
        }

        proc->user_stack_top = (uint64_t) user_stack_top;
        proc->heap_start = options.heap_start ? options.heap_start : 0x400000;
        proc->heap_end = proc->heap_start;
        proc->heap_size = options.heap_size;

        const kernel::threading::ThreadCreateParams thread_params = {
            .name = "main_thread",
            .priority = options.priority,
            .cpu_id = options.cpu_id,
            .stack_size = options.stack_size ? options.stack_size : THREAD_STACK_SIZE,
            .custom_stack = nullptr,
            .is_idle_thread = false,
            .is_user_thread = !options.is_kernel_process,
            .process = proc,
        };

        // Create main thread
        if (options.is_kernel_process) {
            proc->main_thread = threading::ThreadFactory::create_kernel_thread(
                thread_params, (void(*)(void *)) entry_point, nullptr);
        } else {
            proc->main_thread =
                    threading::ThreadFactory::create_user_thread(thread_params, entry_point, user_stack_top);
        }

        if (!proc->main_thread) {
            memory::free_page(proc);
            return nullptr;
        }

        proc->thread_list = proc->main_thread;
        proc->main_thread->next_in_process = nullptr;

        add_process_to_list(proc);
        update_process_statistics(proc, true);
        update_thread_statistics(proc->main_thread, true);

        scheduling::add_thread(proc->main_thread);

        Log::Info("Created process PID=%llu (%s)", proc->pid, proc->name);
        return proc;
    }

    kprocess_t *Manager::create_process_from_elf(const ProcessCreateOptions &options,
                                                 const char *elf_path) {
        if (!initialized) return nullptr;

        auto *proc = (kprocess_t *) kernel::memory::request_page();
        if (!proc) return nullptr;

        memset(proc, 0, sizeof(kprocess_t));

        proc->pml4 = memory::create_user_pagetable();
        proc->heap_start = options.heap_start ? options.heap_start : 0x400000;
        proc->heap_end = proc->heap_start;
        proc->heap_size = options.heap_size;

        ProcessMemoryManager mem_manager(proc);
        proc->memory_manager = &mem_manager;

        ElfLoader loader;
        auto result = loader.load_elf_binary(elf_path, 0x400000, proc->memory_manager);
        if (!result.success) {
            Log::Error("Failed to load ELF: %s - %s", elf_path, result.error_message);
            kernel::memory::free_page(proc);
            return nullptr;
        }
        Log::LogMsg("entry: %p", result.entry_point);

        // Allocate stack
        uint64_t stack_size = options.stack_size ? options.stack_size : 0x4000;
        void *phys = kernel::memory::request_pages(stack_size / PAGE_SIZE);
        if (!phys) {
            kernel::memory::free_page(proc);
            return nullptr;
        }

        kernel::memory::map_range(phys, phys, stack_size, (1ULL << PT_Flag::UserSuper), proc);
        void *user_stack_top = (char *) phys + stack_size;

        proc->pid = next_pid++;
        strncpy(proc->name, options.name, sizeof(proc->name) - 1);
        proc->state = PROCESS_READY;
        proc->user_stack_top = (uint64_t) user_stack_top;
        proc->creation_time = time::get_uptime_ms();
        proc->is_kernel_process = false;

        const threading::ThreadCreateParams thread_params = {
            .name = "main_thread",
            .priority = options.priority,
            .cpu_id = options.cpu_id,
            .stack_size = options.stack_size ? options.stack_size : THREAD_STACK_SIZE,
            .custom_stack = nullptr,
            .is_idle_thread = false,
            .is_user_thread = true,
            .process = proc,
        };

        proc->main_thread = threading::ThreadFactory::create_user_thread(
            thread_params, (void *) result.entry_point, user_stack_top);
        if (!proc->main_thread) {
            kernel::memory::free_page(proc->pml4);
            kernel::memory::free_page(proc);
            return nullptr;
        }

        proc->main_thread->next_in_process = nullptr;
        proc->thread_list = proc->main_thread;

        add_process_to_list(proc);
        update_process_statistics(proc, true);
        update_thread_statistics(proc->main_thread, true);

        scheduling::add_thread(proc->main_thread);

        Log::Info("Created ELF process PID=%llu (%s) from %s", proc->pid, proc->name, elf_path);
        return proc;
    }

    kprocess_t *Manager::create_kernel_process(const ProcessCreateOptions &options,
                                               void (*entry_point)(void *), void *arg) {
        if (!initialized) return nullptr;

        auto *proc = (kprocess_t *) memory::request_page();
        if (!proc) return nullptr;

        memset(proc, 0, sizeof(kprocess_t));

        proc->pid = next_pid++;
        strncpy(proc->name, options.name, sizeof(proc->name) - 1);
        proc->state = PROCESS_READY;
        proc->creation_time = time::get_uptime_ms();
        proc->is_kernel_process = true;

        proc->pml4 = (PageTable *) memory::get_pagetable_address(); // kernel pml4

        proc->heap_start = 0;
        proc->heap_end = 0;
        proc->heap_size = 0;

        const kernel::threading::ThreadCreateParams thread_params = {
            .name = "main_thread",
            .priority = options.priority,
            .cpu_id = options.cpu_id,
            .stack_size = options.stack_size ? options.stack_size : THREAD_STACK_SIZE,
            .custom_stack = nullptr,
            .is_idle_thread = false,
            .is_user_thread = false,
            .process = proc,
        };

        proc->main_thread = threading::ThreadFactory::create_kernel_thread(
            thread_params, entry_point, arg);
        if (!proc->main_thread) {
            memory::free_page(proc);
            return nullptr;
        }

        proc->main_thread->next_in_process = nullptr;
        proc->thread_list = proc->main_thread;

        add_process_to_list(proc);
        update_process_statistics(proc, true);
        update_thread_statistics(proc->main_thread, true);

        scheduling::add_thread(proc->main_thread);

        Log::Info("Created kernel process PID=%llu (%s)", proc->pid, proc->name);
        return proc;
    }

    bool Manager::terminate_process(uint64_t pid, int exit_code) {
        kprocess_t *proc = find_process(pid);
        if (!proc) return false;

        Log::Info("Terminating process PID=%llu (%s) with exit code %d",
                  proc->pid, proc->name, exit_code);

        proc->exit_code = exit_code;
        proc->state = PROCESS_ZOMBIE;

        // Terminate all threads
        kthread_t *thread = proc->thread_list;
        while (thread) {
            thread->state = THREAD_TERMINATED;
            kernel::scheduling::remove_thread(thread);
            thread = thread->next;
        }

        update_statistics();
        return true;
    }

    bool Manager::kill_process(uint64_t pid) {
        kprocess_t *proc = find_process(pid);
        if (!proc) return false;

        Log::Info("Killing process PID=%llu (%s)", proc->pid, proc->name);
        cleanup_process_internal(proc);
        return true;
    }

    void Manager::cleanup_process(kprocess_t *proc) {
        cleanup_process_internal(proc);
    }

    kprocess_t *Manager::find_process(uint64_t pid) {
        kprocess_t *cur = process_list_head;
        while (cur) {
            if (cur->pid == pid) return cur;
            cur = cur->next;
        }
        return nullptr;
    }

    kthread_t *Manager::find_thread(uint64_t tid) {
        kprocess_t *proc = process_list_head;
        while (proc) {
            kthread_t *thread = proc->thread_list;
            while (thread) {
                if (thread->tid == tid) return thread;
                thread = thread->next_in_process;
            }
            proc = proc->next;
        }
        return nullptr;
    }

    ProcessStatistics Manager::get_statistics() {
        update_statistics();
        return stats;
    }

    void Manager::update_statistics() {
        uint64_t total_processes_created_temp = stats.total_processes_created;
        uint64_t total_threads_created_temp = stats.total_threads_created;
        memset(&stats, 0, sizeof(ProcessStatistics));
        stats.total_processes_created = total_processes_created_temp;
        stats.total_threads_created = total_threads_created_temp;

        kprocess_t *proc = process_list_head;
        while (proc) {
            stats.active_processes++;

            if (proc->is_kernel_process) {
                stats.kernel_processes++;
            } else {
                stats.user_processes++;
            }

            if (proc->state == PROCESS_ZOMBIE) {
                stats.zombie_processes++;
            }

            stats.memory_used_by_processes += get_process_memory_usage(proc->pid);
            stats.cpu_time_total += proc->cpu_time;

            // Count threads
            kthread_t *thread = proc->thread_list;
            while (thread) {
                stats.active_threads++;

                switch (thread->state) {
                    case THREAD_RUNNING:
                        stats.running_threads++;
                        break;
                    case THREAD_READY:
                        stats.ready_threads++;
                        break;
                    case THREAD_BLOCKED:
                        stats.blocked_threads++;
                        break;
                    case THREAD_TERMINATED:
                        stats.terminated_threads++;
                        break;
                    default: ;
                }

                thread = thread->next_in_process;
            }

            proc = proc->next;
        }
    }

    ProcessIterator Manager::begin_processes() {
        return ProcessIterator(process_list_head);
    }

    ProcessIterator Manager::end_processes() {
        return ProcessIterator(nullptr);
    }

    ThreadIterator Manager::begin_threads() {
        return ThreadIterator(process_list_head, nullptr);
    }

    ThreadIterator Manager::end_threads() {
        return ThreadIterator(nullptr, nullptr);
    }

    void Manager::cleanup_zombie_processes() {
        kprocess_t *proc = process_list_head;
        kprocess_t *next;

        while (proc) {
            next = proc->next;
            if (proc->state == PROCESS_ZOMBIE && all_threads_from_proc_terminated(proc)) {
                cleanup_process_internal(proc);
            }
            proc = next;
        }
    }

    // Private helper functions
    void Manager::add_process_to_list(kprocess_t *proc) {
        if (!process_list_head) {
            process_list_head = proc;
            proc->next = nullptr;
            return;
        }
        kprocess_t *cur = process_list_head;
        while (cur->next) {
            cur = cur->next;
        }
        cur->next = proc;
        proc->next = nullptr;
    }

    bool Manager::all_threads_from_proc_terminated(kprocess_t *proc) {
        kthread_t *t = proc->thread_list;
        while (t) {
            if (t->state != THREAD_TERMINATED) {
                return false;
            }
            t = t->next;
        }
        return true;
    }

    void Manager::remove_process_from_list(kprocess_t *proc) {
        if (!proc || !process_list_head) return;

        if (process_list_head == proc) {
            process_list_head = proc->next;
            proc->next = nullptr;
            return;
        }

        kprocess_t *cur = process_list_head;
        while (cur->next && cur->next != proc) {
            cur = cur->next;
        }

        if (cur->next == proc) {
            cur->next = proc->next;
            proc->next = nullptr;
        }
    }

    void Manager::cleanup_process_internal(kprocess_t *proc) {
        Log::Info("Cleaning up process PID=%llu (%s)", proc->pid, proc->name);

        update_process_statistics(proc, false);

        kthread_t *t = proc->thread_list;
        while (t) {
            kthread_t *next = t->next;
            kernel::scheduling::remove_thread(t);
            kernel::scheduling::thread_manager::cleanup_thread(t);
            update_thread_statistics(t, false);
            t = next;
        }
        proc->thread_list = nullptr;

        remove_process_from_list(proc);

        if (!proc->is_kernel_process) {
            proc->memory_manager->cleanup_process_pages();
            kernel::memory::free_page(proc->pml4);
        }

        stats.memory_used_by_processes -= proc->memory_usage;
        kernel::memory::free_page(proc);
    }

    void Manager::update_process_statistics(kprocess_t *proc, bool increment) {
        if (increment) {
            stats.total_processes_created++;
        }
    }

    void Manager::update_thread_statistics(kthread_t *thread, bool increment) {
        if (increment) {
            stats.total_threads_created++;
        }
    }

    uint64_t Manager::get_process_memory_usage(uint64_t pid) {
        kprocess_t *proc = find_process(pid);
        if (!proc) return 0;

        uint64_t total = 0;
        kthread_t *thread = proc->thread_list;
        while (thread) {
            total += thread->stack_size;

            thread = thread->next_in_process;
        }

        total += proc->heap_end - proc->heap_start;
        return total; // heap + stack estimate
    }

    namespace util {
        ProcessStatistics get_system_stats() {
            return Manager::get_statistics();
        }

        void print_process_list() {
            Log::Info("=== Process List ===");
            for (auto it = Manager::begin_processes(); it != Manager::end_processes(); ++it) {
                ProcessInfo info = *it;
                Log::Info("PID: %llu, Name: %s, State: %d, Threads: %llu, Memory: %llu KB",
                          info.pid, info.name, info.state, info.thread_count, info.memory_usage / 1024);
            }
        }

        void print_thread_list() {
            Log::Info("=== Thread List ===");
            for (auto it = Manager::begin_threads(); it != Manager::end_threads(); ++it) {
                ThreadInfo info = *it;
                Log::Info("TID: %llu, PID: %llu, State: %d, CPU: %d, Priority: %d",
                          info.tid, info.pid, info.state, info.cpu_id, info.priority);
            }
        }

        double get_cpu_usage_percent() {
            ProcessStatistics stats = get_system_stats();
            if (stats.active_threads == 0) return 0.0;
            return (double) stats.running_threads / stats.active_threads * 100.0;
        }

        uint64_t get_total_memory_usage() {
            return get_system_stats().memory_used_by_processes;
        }
    }
} // namespace kernel::process
