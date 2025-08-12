// process.cpp
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

#include "process.h"

#include "../../include/log.h"
#include "../../include/string.h"
#include "../scheduling/thread.h"
#include "../exec/elf.h"
#include "../include/memory.h"

void add_process_to_list(kprocess_t* proc) {
    if (!process_list_head) {
        process_list_head = proc;
        proc->next = nullptr;
        return;
    }
    kprocess_t* cur = process_list_head;
    while (cur->next) {
        cur = cur->next;
    }
    cur->next = proc;
    proc->next = nullptr;
}

void remove_process_from_list(kprocess_t *proc) {
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



kprocess_t* find_process_by_pid(uint64_t pid) {
    kprocess_t* cur = process_list_head;
    while (cur) {
        if (cur->pid == pid) return cur;
        cur = cur->next;
    }
    return nullptr;
}

bool all_threads_terminated(kprocess_t *proc) {
    kthread_t *t = proc->thread_list;
    while (t) {
        if (t->state != THREAD_TERMINATED) {
            return false;
        }
        t = t->next;
    }
    return true;
}


kprocess_t* create_process(const char* name, void* entry_point, void* user_stack_top) {
    kprocess_t* proc = (kprocess_t*)kernel::memory::request_page();
    memset(proc, 0, sizeof(kprocess_t));

    proc->pid = next_pid++;
    strncpy(proc->name, name, sizeof(proc->name) - 1);
    proc->state = PROCESS_READY;

    // Create user-space PML4
    proc->pml4 = kernel::memory::create_user_pagetable();

    proc->user_stack_top = (uint64_t)user_stack_top;
    proc->heap_start = 0x400000; // example: start after code
    proc->heap_end = proc->heap_start;

    // Create main thread bound to this process
    proc->main_thread = create_user_thread(entry_point, user_stack_top);
    proc->main_thread->process = proc; // requires adding `process` pointer to kthread_t

    proc->thread_list = proc->main_thread;

    return proc;
}

kprocess_t* create_process_from_elf(const char* name, const char* path) {
    uint64_t entry;
    void* base_addr = load_elf_binary(path, &entry, 0x400000);
    if (!base_addr) {
        Log::Error("Failed to load ELF: %s", path);
        return nullptr;
    }
    Log::debug("base address: %p entry: %p", base_addr, entry);

    void* phys = kernel::memory::request_pages(4);
    kernel::memory::map_range(phys, phys, 0x4000, PT_Flag::UserSuper);
    void* user_stack_top = phys + 0x4000;

    kprocess_t* proc = (kprocess_t*)kernel::memory::request_page();
    memset(proc, 0, sizeof(kprocess_t));

    proc->pid = next_pid++;
    strncpy(proc->name, name, sizeof(proc->name)-1);
    proc->state = PROCESS_READY;

    proc->pml4 = kernel::memory::create_user_pagetable();
    proc->main_thread = create_user_thread((void*)entry, user_stack_top);
    proc->main_thread->process = proc;
    proc->thread_list = proc->main_thread;

    add_process_to_list(proc);
    return proc;
}

void cleanup_process(kprocess_t *proc) {
    Log::Info("Cleaning up process PID=%d (%s)", proc->pid, proc->name);

    kthread_t *t = proc->thread_list;
    while (t) {
        kthread_t *next = t->next;
        kernel::memory::free_page(t);
        t = next;
    }
    proc->thread_list = nullptr;

    remove_process_from_list(proc);

    kernel::memory::free_user_pagetable(proc->pml4);

    kernel::memory::free_page(proc->pml4);

    kernel::memory::free_page(proc);
}
