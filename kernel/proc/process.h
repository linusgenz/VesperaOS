// process.h
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

#ifndef PROCESS_H
#define PROCESS_H
#include <cstdint>
#include "../memory/page_table_manager.h"

enum ProcessState {
    PROCESS_NEW = 0,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_ZOMBIE,
    PROCESS_TERMINATED
};

struct user_page {
    void* phys_addr;
    user_page* next;
};

struct kprocess_t {
    uint64_t pid;
    char name[64];
    ProcessState state;

    // Memory management
    PageTable *pml4;
    uint64_t user_stack_top;
    uint64_t heap_start;
    uint64_t heap_end;
    uint64_t heap_size;
    uint64_t memory_usage;

    // Page management
    user_page* user_pages_head;

    // Thread management
    struct kthread_t *main_thread;
    struct kthread_t *thread_list;
    uint32_t thread_count;

    // Process tree
    struct kprocess_t *parent;
    struct kprocess_t *first_child;
    struct kprocess_t *next_sibling;

    // Linked list for global process management
    struct kprocess_t *next;

    // Statistics and monitoring
    uint64_t creation_time;
    uint64_t cpu_time;
    uint64_t cpu_time_user;
    uint64_t cpu_time_kernel;
    uint32_t context_switches;

    // Exit information
    int exit_code;
    bool is_kernel_process;

    // File descriptors (for future implementation)
    void *fd_table;

    // Signals (for future implementation)
    uint64_t signal_mask;
    void *signal_handlers;
} __attribute__((packed));

#endif //PROCESS_H
