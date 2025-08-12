// process.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 11.08.25.
//
// This file is part of LuminOS.
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
#include "cstdint"
#include "../memory/page_table_manager.h"
struct kthread_t; // forward

enum ProcessState {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
};

struct kprocess_t {
    uint64_t pid;
    char name[32];

    ProcessState state;

    // Memory
    PageTable* pml4;         // Root page table for the process
    uint64_t user_stack_top; // Virtual top of user stack
    uint64_t heap_start;     // Start of heap
    uint64_t heap_end;       // Current end of heap

    // Thread list
    kthread_t* main_thread;
    kthread_t* thread_list; // Linked list of threads belonging to this process

    // Open files / resources
    void* file_table; // placeholder

    // Scheduling
    uint8_t priority;
    uint8_t cpu_affinity;

    int64_t exit_code;

    kprocess_t* next;
};

inline kprocess_t* process_list_head = nullptr;
inline uint64_t next_pid = 1;

void add_process_to_list(kprocess_t* proc);
kprocess_t* find_process_by_pid(uint64_t pid);
kprocess_t* create_process(const char* name, void* entry_point, void* user_stack_top);
kprocess_t* create_process_from_elf(const char* name, const char* path);
void cleanup_process(kprocess_t *proc);
bool all_threads_terminated(kprocess_t *proc);
#endif //PROCESS_H
