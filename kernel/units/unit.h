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
#include <vespera/signals.h>

#include "execution_context.h"
#include "unit_handle_set.h"
#include "vm_area_list.h"
#include "vespera/sync/wait_queue.h"

#define MAX_UNIT_HANDLE_SLOTS 64

class Realm;
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

// SleepContext — transient state while a unit is sleeping.
struct SleepContext {
    u64 wakeup_ns;
    bool interrupted = false;
};

// ---------------------------------------------------------------------------
// Unit — kernel-internal representation of a schedulable execution context.
//
// Ownership model:
//   - UnitManager owns the static Unit array and is the sole allocator.
//   - The parent Realm holds a non-owning pointer to the Unit via unit_list.
//   - unit_handle_set_ tracks which handles this unit holds; the Realm-level
//     HandleTable owns the actual resources and reference counts.
//   - vm_areas_ owns its VmArea chain and frees it on destroy.
//
// Struct layout:
//   [1] Identity & linkage
//   [2] Scheduling & classification
//   [3] Execution context
//   [4] Virtual memory areas
//   [5] Handle set
//   [6] Signal state
// ---------------------------------------------------------------------------
class Unit {
   public:
    UnitId id{0};
    RealmId rid{0};
    Realm* parent{nullptr};
    char* name{nullptr};

    // Intrusive list links: next is the global UnitManager chain,
    // realm_next is the per-Realm chain.
    Unit* next{nullptr};
    Unit* realm_next{nullptr};

    UnitState state{UnitState::New};
    u64 creation_time{0};

    uptr cr3{0};

    u8 priority{0};
    u8 cpu_id{0};
    bool active{false};

    bool is_idle{false};
    bool is_user{false};
    bool is_main_unit{false};
    bool is_kernel{false};

    u64 cpu_time_ns{0};
    u64 run_start_ns{0};

    int exit_code{0};
    u32 user_stack_slot{0};

    ExecutionContext context{};
    SleepContext sleep_context{};

    u64 heap_start{};
    u64 heap_end{};

    UnitId joiner_id{0};
    WaitQueue wait_queue;

    u64 signals_pending{0};
    u64 signals_masked{0};
    SignalAction signal_actions[32]{};
    phys_addr_t tls_phys;
    uptr tls_vaddr;
    usize tls_pages;

    uptr futex_uaddr{0};

    Unit() {
        handle_set_.init();
    }

    Unit(const Unit&) = delete;
    Unit& operator=(const Unit&) = delete;

    void add_vma(kernel::units::VmArea* vma) {
        vm_areas_.add(vma);
    }
    [[nodiscard]] kernel::units::VmArea* find_vma(uptr addr, usize len) const {
        return vm_areas_.find(addr, len);
    }
    bool remove_vma(uptr addr, usize len) {
        return vm_areas_.remove(addr, len);
    }
    void free_vma_list() {
        vm_areas_.free_all();
    }
    [[nodiscard]] kernel::units::VmArea* get_vma_list() const {
        return vm_areas_.head();
    }

    [[nodiscard]] i64 attach_handle(HandleId h) {
        return handle_set_.attach(h);
    }
    [[nodiscard]] i64 detach_handle(HandleId h) {
        return handle_set_.detach(h);
    }
    void detach_all_handles() {
        handle_set_.detach_all();
    }
    [[nodiscard]] u32 find_handle_slot(HandleId h) const {
        return handle_set_.find_slot(h);
    }
    [[nodiscard]] u64 handle_count() const {
        return handle_set_.count();
    }

   private:
    kernel::units::VmAreaList vm_areas_;
    kernel::units::UnitHandleSet handle_set_;
};

#endif  // VESPERAOS_UNIT_H