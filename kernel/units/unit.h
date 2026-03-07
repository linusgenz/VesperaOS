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

#define MAX_UNIT_HANDLE_SLOTS 64


/**
 * @brief Kernel-internal Unit lifecycle state.
 *
 * Mirrors the UNIT_STATE_* constants from uapi/vespera/unit_info.h with a
 * type-safe enum class. Always cast to uint8_t when writing into
 * a unit_info_t struct.
 */
enum class UnitState : uint8_t {
    New = UNIT_STATE_NEW,
    Ready = UNIT_STATE_READY,
    Running = UNIT_STATE_RUNNING,
    Blocked = UNIT_STATE_BLOCKED,
    Zombie = UNIT_STATE_ZOMBIE,
    Terminated = UNIT_STATE_TERMINATED,
};

struct ArgRegisters {
    uint64_t rdi, rsi, rdx, rcx, r8, r9;
};

/// @warning when changing this struct syscall might break as offsets are hardcoded!
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
    HandleId handle;

    VmArea* next;
};

struct UnitHandleTable {
    HandleId slots[MAX_UNIT_HANDLE_SLOTS];
    uint64_t count;
    Spinlock lock;
};

class Unit {
   private:
    UnitHandleTable handle_table_{};
    VmArea* vma_list_{};

   public:
    UnitId id{0};
    RealmId rid{0};
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

    int64_t attach_handle(HandleId h) {
        handle_table_.lock.lock();
        if (handle_table_.count >= MAX_UNIT_HANDLE_SLOTS) {
            handle_table_.lock.unlock();
            return -ENOMEM;
        }
        for (unsigned long& slot : handle_table_.slots) {
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

    int64_t detach_handle(HandleId h) {
        handle_table_.lock.lock();
        for (unsigned long& slot : handle_table_.slots) {
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

    int64_t detach_all_handles() {
        handle_table_.lock.lock();
        for (uint64_t& slot : handle_table_.slots) {
            if (const HandleId h = slot; h != 0) {
                slot = 0;
            }
        }
        handle_table_.count = 0;
        handle_count = 0;

        handle_table_.lock.unlock();
        return SUCCESS_CODE;
    }

    [[nodiscard]] uint32_t find_handle_slot(HandleId h) const {
        for (uint32_t i = 0; i < MAX_UNIT_HANDLE_SLOTS; ++i) {
            if (handle_table_.slots[i] == h) return i;
        }
        return -1;
    }
};

#endif  // VESPERAOS_UNIT_H
