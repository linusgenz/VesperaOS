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

#include <uapi/vespera/dev/unit_info.h>
#include <vespera/realm/realm.h>
#include <vespera/signals.h>

#include "../scheduling/unit_context.h"

#define MAX_UNIT_HANDLE_SLOTS 64

/**
 * @brief Kernel-internal Unit lifecycle state.
 *
 * Mirrors the UNIT_STATE_* constants from uapi/vespera/unit_info.h with a
 * type-safe enum class. Always cast to u8 when writing into
 * a unit_info_t struct.
 */
enum class UnitState : u8 {
    New = UNIT_STATE_NEW,
    Ready = UNIT_STATE_READY,
    Running = UNIT_STATE_RUNNING,
    Blocked = UNIT_STATE_BLOCKED,
    Zombie = UNIT_STATE_ZOMBIE,
    Terminated = UNIT_STATE_TERMINATED,
};

struct ArgRegisters {
    u64 rdi, rsi, rdx, rcx, r8, r9;
};

/// @warning when changing this struct syscall might break as offsets are hardcoded!
// TODO refactor this struct
typedef struct ExecutionContext {
    u64 stack_size;
    virt_addr_t stack;
    virt_addr_t stack_top;
    virt_addr_t stack_pointer;
    u64 user_stack_size;
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

    TrapFrame current_trap_frame;

    UnitCpuContext cpu_ctx;
    UnitFpuState fpu_ctx;
} execution_context_t;

struct SleepContext {
    u64 wakeup_ns;
    bool interrupted = false;
};

struct VmArea {
    uptr start;
    usize length;
    u64 prot;
    u64 flags;
    uptr file_off;
    HandleId handle;

    VmArea* next;
};

struct UnitHandleTable {
    HandleId slots[MAX_UNIT_HANDLE_SLOTS];
    u64 count;
    Spinlock lock;
};

class Unit {
   private:
    UnitHandleTable handle_table_{};
    VmArea* vma_list_{};

   public:
    UnitId id{0};
    RealmId rid{0};
    char* name{nullptr};

    Unit* next{nullptr};
    Unit* realm_next{};

    UnitState state{UnitState::New};
    u64 creation_time{0};

    u8 priority{0};
    u8 cpu_id{0};

    int exit_code{0};
    bool active{false};

    bool is_idle{false};
    bool is_user{false};
    bool is_main_unit{false};
    bool is_kernel{false};

    u64 cpu_time_ns  = 0;
    u64 run_start_ns = 0;

    u64 heap_start{};
    u64 heap_end{};

    u32 user_stack_slot{0};

    u64 handle_count{0};

    execution_context_t context{};
    SleepContext sleep_context{};

    u64 signals_pending;
    u64 signals_masked;
    SignalAction signal_actions[32];

    Unit() {
        memset(&handle_table_, 0, sizeof(handle_table_));
        handle_table_.lock.init();
    }

    void add_vma(VmArea* vma) {
        vma->next = vma_list_;
        vma_list_ = vma;
    }

    [[nodiscard]] VmArea* find_vma(uptr addr, usize len) const {
        for (VmArea* v = vma_list_; v; v = v->next) {
            if (addr >= v->start && (addr + len) <= (v->start + v->length)) {
                return v;
            }
        }
        return nullptr;
    }

    [[nodiscard]] VmArea* get_vma_list() const {
        return vma_list_;
    }

    bool remove_vma(uptr addr, usize len) {
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

    void free_vma_list() {
        VmArea* next = nullptr;
        VmArea* cur = vma_list_;

        while (cur) {
            next = cur->next;
            delete cur;
            cur = next;
        }

        vma_list_ = nullptr;
    }

    i64 attach_handle(HandleId h) {
        handle_table_.lock.lock();
        if (handle_table_.count >= MAX_UNIT_HANDLE_SLOTS) {
            handle_table_.lock.unlock();
            return -ENOMEM;
        }
        for (HandleId& slot : handle_table_.slots) {
            if (slot == 0) {
                slot = h;
                handle_table_.count++;
                handle_count = handle_table_.count;
                handle_table_.lock.unlock();
                return SUCCESS_CODE;
            }
        }
        handle_table_.lock.unlock();
        return -ENOMEM;
    }

    i64 detach_handle(HandleId h) {
        handle_table_.lock.lock();
        for (HandleId& slot : handle_table_.slots) {
            if (slot == h) {
                slot = 0;
                handle_table_.count--;
                handle_count = handle_table_.count;
                handle_table_.lock.unlock();
                return SUCCESS_CODE;
            }
        }
        handle_table_.lock.unlock();
        return -EBADH;
    }

    i64 detach_all_handles() {
        handle_table_.lock.lock();
        for (u64& slot : handle_table_.slots) {
            if (const HandleId h = slot; h != 0) {
                slot = 0;
            }
        }
        handle_table_.count = 0;
        handle_count = 0;

        handle_table_.lock.unlock();
        return SUCCESS_CODE;
    }

    [[nodiscard]] u32 find_handle_slot(HandleId h) const {
        for (u32 i = 0; i < MAX_UNIT_HANDLE_SLOTS; ++i) {
            if (handle_table_.slots[i] == h) return i;
        }
        return -1;
    }
};

#endif  // VESPERAOS_UNIT_H
