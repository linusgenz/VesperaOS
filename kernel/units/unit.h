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

#include <vespera/realm/realm.h>

#include "../types/handle.h"
#include "../types/types.h"

enum class UnitState : uint8_t { New, Ready, Running, Blocked, Zombie, Terminated };

struct ArgRegisters {
    uint64_t rdi, rsi, rdx, rcx, r8, r9;
};

// WARNING when changing this struct syscall might break as offsets are hardcoded!
// TODO refactor this struct
typedef struct ExecutionContext {
    uint64_t stack_size;
    virt_addr_t stack;
    virt_addr_t stack_top;
    virt_addr_t stack_pointer;
    uint64_t user_stack_size;
    virt_addr_t user_stack;
    virt_addr_t user_stack_top;
    virt_addr_t user_stack_pointer;

    void (*entry)(void*);

    ArgRegisters regs;

    bool initialized;

    void* arg;
    void* saved_user_rsp;
    void* kernel_rsp;
    bool from_syscall;
    void* kernel_rsp_after_syscall;

    phys_addr_t user_stack_phys;
    virt_addr_t user_stack_virt_base;
} execution_context_t;

typedef struct SleepContext {
    uint64_t wakeup_tick;
    void* kernel_rsp_after_sleep;
} sleep_context_t;

struct VmArea {
    uintptr_t start;
    size_t length;
    uint64_t prot;
    uint64_t flags;
    uintptr_t file_off;
    handle_id_t handle;

    VmArea* next;
};

class Unit {
   private:
    unit_handle_table_t handle_table_{};
    VmArea* vma_list_{};

   public:
    unit_id_t id{0};
    realm_id_t rid{0};
    const char* name{nullptr};

    Unit* next{nullptr};
    Unit* realm_next{};

    UnitState state{UnitState::New};
    uint64_t creation_time{0};

    uint8_t priority{0};
    uint8_t cpu_id{0};

    int exit_code{0};
    bool active{false};

    bool is_idle{false};
    bool is_user{false};
    bool is_kernel{false};

    uint64_t heap_end{};

    uint64_t handle_count{0};

    execution_context_t context{};
    sleep_context_t sleep_context{};

    Unit() {
        memset(&handle_table_, 0, sizeof(handle_table_));
        handle_table_.lock.init();
    }

    void add_vma(VmArea* vma) {
        vma->next = vma_list_;
        vma_list_ = vma;
    }

    [[nodiscard]] VmArea* find_vma(uintptr_t addr, size_t len) const {
        for (VmArea* v = vma_list_; v; v = v->next) {
            if (addr >= v->start && (addr + len) <= (v->start + v->length)) {
                return v;
            }
        }
        return nullptr;
    }

    bool remove_vma(uintptr_t addr, size_t len) {
        VmArea* prev = nullptr;
        VmArea* cur = vma_list_;

        while (cur) {
            if (cur->start == addr && cur->length == len) {
                if (prev)
                    prev->next = cur->next;
                else
                    vma_list_ = cur->next;
                delete cur;  // Achtung: später evtl. eigener Allocator
                return true;
            }
            prev = cur;
            cur = cur->next;
        }
        return false;
    }

    error_code_t attach_handle(handle_id_t h) {
        handle_table_.lock.lock();
        if (handle_table_.count >= MAX_UNIT_HANDLE_SLOTS) {
            handle_table_.lock.unlock();
            return MOD_ERR_OUT_OF_MEMORY;
        }
        for (unsigned long& slot : handle_table_.slots) {
            if (slot == 0) {
                slot = h;
                handle_table_.count++;
                handle_count = handle_table_.count;
                handle_table_.lock.unlock();
                return MOD_SUCCESS;
            }
        }
        handle_table_.lock.unlock();
        return MOD_ERR_OUT_OF_MEMORY;
    }

    error_code_t detach_handle(handle_id_t h) {
        handle_table_.lock.lock();
        for (unsigned long& slot : handle_table_.slots) {
            if (slot == h) {
                slot = 0;
                handle_table_.count--;
                handle_count = handle_table_.count;
                handle_table_.lock.unlock();
                return MOD_SUCCESS;
            }
        }
        handle_table_.lock.unlock();
        return MOD_ERR_INVALID_HANDLE;
    }

    error_code_t detach_all_handles() {
        handle_table_.lock.lock();
        for (uint64_t& slot : handle_table_.slots) {
            if (const handle_id_t h = slot; h != 0) {
                slot = 0;
            }
        }
        handle_table_.count = 0;
        handle_count = 0;

        handle_table_.lock.unlock();
        return MOD_SUCCESS;
    }

    [[nodiscard]] uint32_t find_handle_slot(handle_id_t h) const {
        for (uint32_t i = 0; i < MAX_UNIT_HANDLE_SLOTS; ++i) {
            if (handle_table_.slots[i] == h) return i;
        }
        return -1;
    }
};

#endif  // VESPERAOS_UNIT_H
