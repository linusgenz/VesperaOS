// unit.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 19.09.25.
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

#ifndef VESPERAOS_UNIT_H
#define VESPERAOS_UNIT_H

#include "../realm/realm.h"
#include "../types/types.h"
#include "../types/handle.h"

struct realm;

typedef enum {
    UNIT_NEW,
    UNIT_READY,
    UNIT_RUNNING,
    UNIT_BLOCKED,
    UNIT_ZOMBIE,
    UNIT_TERMINATED
} UnitState;

typedef struct arg_registers {
    uint64_t rdi, rsi, rdx, rcx, r8, r9;
} arg_registers_t;


// WARINING when changing this struct syscall might break as offsets are hardcoded!
typedef struct execution_context {
    uint64_t stack_size;
    void *stack;
    void *stack_top;
    void *stack_pointer;
    uint64_t user_stack_size;
    void *user_stack;
    void *user_stack_top;
    void *user_stack_pointer;

    void (*entry)(void *);

   arg_registers_t regs;

    bool initialized;

    void *arg;
    void *saved_user_rsp;
    void *kernel_rsp;
    bool from_syscall;
    void *kernel_rsp_after_syscall;
} execution_context_t;

typedef struct sleep_context {
    uint64_t wakeup_tick;
    void *kernel_rsp_after_sleep;
} sleep_context_t;

struct VmArea {
    uintptr_t start;
    size_t length;
    uint64_t prot;
    uint64_t flags;
    uintptr_t file_off;
    HandleID handle;

    VmArea* next;
};


class Unit {
private:
    unit_handle_table_t handle_table;
    VmArea* vma_list;
public:
    UnitID id;
    RealmID rid;
    const char *name;

    Unit *next;

    UnitState state;
    uint64_t creation_time;

    uint8_t priority;
    uint8_t cpu_id;

    int exit_code;
    bool active;

    bool is_idle;
    bool is_user;
    bool is_kernel;

    uint64_t heap_end;

    uint64_t handle_count;

    execution_context_t context;
    sleep_context_t sleep_context;

    Unit() : id(0), rid(0), name(nullptr), next(nullptr),
             state(UnitState::UNIT_NEW), creation_time(0),
             priority(0), cpu_id(0),
             exit_code(0), active(false),
             is_idle(false), is_user(false), is_kernel(false),
             handle_count(0) {
        memset(&handle_table, 0, sizeof(handle_table));
        handle_table.lock.init();
    }

    void add_vma(VmArea* vma) {
        vma->next = vma_list;
        vma_list = vma;
    }

    VmArea* find_vma(uintptr_t addr, size_t len) {
        for (VmArea* v = vma_list; v; v = v->next) {
            if (addr >= v->start && (addr + len) <= (v->start + v->length)) {
                return v;
            }
        }
        return nullptr;
    }

    bool remove_vma(uintptr_t addr, size_t len) {
        VmArea* prev = nullptr;
        VmArea* cur  = vma_list;

        while (cur) {
            if (cur->start == addr && cur->length == len) {
                if (prev) prev->next = cur->next;
                else vma_list = cur->next;
                delete cur; // Achtung: später evtl. eigener Allocator
                return true;
            }
            prev = cur;
            cur = cur->next;
        }
        return false;
    }

    ErrorCode attach_handle(HandleID h) {
        handle_table.lock.lock();
        if (handle_table.count >= MAX_UNIT_HANDLE_SLOTS) {
            handle_table.lock.unlock();
            return MOD_ERR_OUT_OF_MEMORY;
        }
        for (size_t i = 0; i < MAX_UNIT_HANDLE_SLOTS; ++i) {
            if (handle_table.slots[i] == 0) {
                handle_table.slots[i] = h;
                handle_table.count++;
                handle_count = handle_table.count;
                handle_table.lock.unlock();
                return MOD_SUCCESS;
            }
        }
        handle_table.lock.unlock();
        return MOD_ERR_OUT_OF_MEMORY;
    }

    ErrorCode detach_handle(HandleID h) {
        handle_table.lock.lock();
        for (size_t i = 0; i < MAX_UNIT_HANDLE_SLOTS; ++i) {
            if (handle_table.slots[i] == h) {
                handle_table.slots[i] = 0;
                handle_table.count--;
                handle_count = handle_table.count;
                handle_table.lock.unlock();
                return MOD_SUCCESS;
            }
        }
        handle_table.lock.unlock();
        return MOD_ERR_INVALID_HANDLE;
    }

    ErrorCode detach_all_handles() {
        handle_table.lock.lock();
        for (uint64_t & slot : handle_table.slots) {
            HandleID h = slot;
            if (h != 0) {
                slot = 0;
            }
        }
        handle_table.count = 0;
        handle_count = 0;

        handle_table.lock.unlock();
        return MOD_SUCCESS;
    }

    [[nodiscard]] int find_handle_slot(HandleID h) const {
        for (size_t i = 0; i < MAX_UNIT_HANDLE_SLOTS; ++i) {
            if (handle_table.slots[i] == h) return i;
        }
        return -1;
    }
};


#endif //VESPERAOS_UNIT_H
