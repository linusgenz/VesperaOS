// unit_manager.cpp
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

#include "unit_manager.h"

#include <uapi/vespera/dev/unit_info.h>
#include <vespera/log.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>

#include "../../filesystem/realmfs/realmfs.h"
#include "../scheduling/schedule_manager.h"
#include <vespera/unit_config.h>

Unit UnitManager::units_[MAX_UNITS];
Spinlock UnitManager::global_lock_;
UnitId UnitManager::next_id_ = 1;
bool UnitManager::initialized_ = false;

constexpr uptr USER_STACK_TOP = 0x00007FFFFFFF0000ULL;

void UnitManager::initialize() {
    global_lock_.init("unit_manager_lock");
    for (auto& unit : units_) {
        unit.active = false;
        unit.id = 0;
        unit.rid = 0;
        unit.next = nullptr;
    }
    next_id_ = 1;
}

bool UnitManager::is_initialized() {
    return initialized_;
}

UnitId UnitManager::allocate_id() {
    return next_id_++;
}

static void* user_virt_to_hhdm(const Unit* u, const uptr user_vaddr) {
    const uptr offset = user_vaddr - virt_raw(u->context.user_stack_virt_base);
    if (offset >= u->context.user_stack_size) return nullptr;
    return virt_ptr(virt_add(u->context.user_stack, offset));
}

static void write_user_ptr(const Unit* u, const uptr user_addr, const uptr val) {
    void* hhdm = user_virt_to_hhdm(u, user_addr);
    if (!hhdm) return;
    *static_cast<uptr*>(hhdm) = val;
}

static void memcpy_to_user(const Unit* u, void* user_dest, const void* src, const usize len) {
    void* hhdm = user_virt_to_hhdm(u, reinterpret_cast<uptr>(user_dest));
    if (!hhdm) return;
    memcpy(hhdm, src, len);
}

uptr setup_user_args_and_env(Unit* u, const char** argv, const char** envp) {
    auto sp = virt_raw(u->context.user_stack_top);

    const char* argv_user[16];
    const char* envp_user[16];

    usize envc = 0;
    while (envp && envp[envc] && envc < 16) envc++;

    for (usize i = envc; i > 0; i--) {
        const usize idx = i - 1;
        const usize len = strlen(envp[idx]) + 1;
        sp -= len;
        sp &= ~0xF;
        memcpy_to_user(u, reinterpret_cast<void*>(sp), envp[idx], len);
        envp_user[idx] = reinterpret_cast<const char*>(sp);
    }

    sp -= sizeof(uptr);
    write_user_ptr(u, sp, 0);

    for (usize i = envc; i > 0; i--) {
        const usize idx = i - 1;
        sp -= sizeof(uptr);
        write_user_ptr(u, sp, reinterpret_cast<uptr>(envp_user[idx]));
    }
    const uptr envp_ptr = sp;

    usize argc = 0;
    while (argv && argv[argc] && argc < 16) argc++;

    for (usize i = argc; i > 0; i--) {
        const usize idx = i - 1;
        const usize len = strlen(argv[idx]) + 1;
        sp -= len;
        sp &= ~0xF;
        memcpy_to_user(u, reinterpret_cast<void*>(sp), argv[idx], len);
        argv_user[idx] = reinterpret_cast<const char*>(sp);
    }

    sp -= sizeof(uptr);
    write_user_ptr(u, sp, 0);

    for (usize i = argc; i > 0; i--) {
        const usize idx = i - 1;
        sp -= sizeof(uptr);
        write_user_ptr(u, sp, reinterpret_cast<uptr>(argv_user[idx]));
    }
    const uptr argv_ptr = sp;

    sp -= sizeof(uptr);
    write_user_ptr(u, sp, argc);

    sp &= ~0xFULL;
    u->context.regs.rdi = argc;
    u->context.regs.rsi = argv_ptr;
    u->context.regs.rdx = envp_ptr;
    u->context.user_stack_pointer = virt_from_raw(sp);

    return sp;
}

Unit* UnitManager::create(const RealmId realm_id, const unit_entry_t entry_point, void* arg, const UnitConfig* cfg) {
    if (!cfg || !entry_point) return nullptr;

    Realm* realm = RealmManager::get(realm_id);
    if (!realm || !realm->active) return nullptr;

    SpinlockGuard g(global_lock_);

    for (auto& i : units_) {
        if (!i.active) {
            Unit* u = &i;
            memset(u, 0, sizeof(*u));

            u->id = allocate_id();
            u->rid = realm_id;
            u->name = cfg->name;
            u->exit_code = 0;
            u->active = true;
            u->next = nullptr;
            u->priority = cfg->priority;
            u->cpu_id = cfg->cpu_id;

            u->is_idle = cfg->is_idle;
            u->is_user = cfg->is_user;
            u->is_kernel = !cfg->is_user;

            u->handle_count = cfg->initial_handle_count;

            u->context.entry = entry_point;
            u->context.arg = arg;

            const u64 stack_size = cfg->stack_size ? cfg->stack_size : DEFAULT_UNIT_STACK_SIZE;

            if (u->is_user) {
                const u64 user_stack_size = cfg->user_stack_size
                                               ? cfg->user_stack_size
                                               : (cfg->stack_size ? cfg->stack_size : DEFAULT_UNIT_STACK_SIZE);
                const usize pages = (user_stack_size + 0xFFF) / 0x1000;

                const phys_addr_t stack_phys = kernel::memory::request_pages_phys(pages);
                if (phys_null(stack_phys)) {
                    u->active = false;
                    return nullptr;
                }

                const virt_addr_t stack_hhdm = phys_to_virt(stack_phys);
                memset(stack_hhdm, 0, user_stack_size);

                const virt_addr_t user_virt_base = virt_from_raw(USER_STACK_TOP - user_stack_size);

                realm->page_table->map_range(
                    user_virt_base,
                    stack_phys,
                    user_stack_size,
                    (1ULL << PtFlag::Present) | (1ULL << PtFlag::ReadWrite) | (1ULL << PtFlag::UserSuper)
                );

                u->context.user_stack = stack_hhdm;
                u->context.user_stack_phys = stack_phys;
                u->context.user_stack_size = user_stack_size;
                u->context.user_stack_virt_base = user_virt_base;
                u->context.user_stack_top = virt_from_raw(USER_STACK_TOP);
                u->context.user_stack_pointer = u->context.user_stack_top;
            }

            u->context.stack = kernel::memory::request_pages((stack_size + 0xFFF) / 0x1000);
            if (virt_null(u->context.stack)) {
                u->active = false;
                return nullptr;
            }
            memset(u->context.stack, 0, stack_size);

            u->context.stack_size = stack_size;
            u->context.stack_top = virt_add(u->context.stack, stack_size);
            u->context.stack_pointer = u->context.stack_top;

            if (u->is_idle || u->is_kernel) {
                setup_kernel_unit_stack(u);
            } else if (u->is_user) {
                setup_user_args_and_env(u, cfg->argv, cfg->envp);
                setup_user_unit_stack(u);
            }

            if (cfg->initial_handles && cfg->initial_handle_count > 0) {
                for (u64 j = 0; j < cfg->initial_handle_count; ++j) {
                    const HandleId h = cfg->initial_handles[j];
                    if (const HandleEntry* he = realm->lookup_handle(h); !he) continue;
                    u->attach_handle(h);
                    realm->acquire_handle(h);
                }
            }

            if (!u->is_idle && cfg->auto_schedule) {
                kernel::scheduling::add_unit(u);
            }

            SpinlockGuard rg(realm->lock);
            u->realm_next = realm->unit_list;
            realm->unit_list = u;
            realm->unit_count++;

            SYS_EVENT_UNIT_CREATED(u->id, u->rid);
            RealmFs::register_unit(u->id, u->name, u, realm->name);
            return u;
        }
    }

    return nullptr;
}

Unit* UnitManager::get(const UnitId id) {
    SpinlockGuard g(global_lock_);
    for (auto& unit : units_) {
        if (unit.active && unit.id == id) return &unit;
    }
    return nullptr;
}

bool UnitManager::destroy(const UnitId id) {
    SpinlockGuard g(global_lock_);

    for (auto& i : units_) {
        if (i.active && i.id == id) {
            Unit* u = &i;
            kernel::scheduling::remove_unit(u);

            Realm* r = RealmManager::get(u->rid);
            if (r) {
                SpinlockGuard rg(r->lock);
                Unit** prev = &r->unit_list;
                while (*prev) {
                    if (*prev == u) {
                        *prev = u->realm_next;
                        r->unit_count--;
                        break;
                    }
                    prev = &(*prev)->realm_next;
                }
            }

            u->detach_all_handles();

            u->free_vma_list();

            if (!virt_null(u->context.stack)) {
                const usize pages = (u->context.stack_size + 0xFFF) / 0x1000;
                kernel::memory::free_pages(u->context.stack, pages);
                u->context.stack = make_virt(nullptr);
            }

            if (u->is_user && !virt_null(u->context.user_stack)) {
                const usize user_pages = (u->context.user_stack_size + 0xFFF) / 0x1000;
                kernel::memory::unmap_range(u->context.user_stack_virt_base, u->context.user_stack_size);
                kernel::memory::free_pages(u->context.user_stack, user_pages);
                u->context.user_stack = make_virt(nullptr);
            }

            SYS_EVENT_UNIT_DESTROYED(u->id, u->rid);
            RealmFs::unregister_unit(id);

            if (r && r->unit_count == 0) RealmManager::destroy(r->id);

            memset(u, 0, sizeof(*u));
            memset(&u->context, 0, sizeof(u->context));

            return true;
        }
    }

    return false;
}

void UnitManager::list() {
    SpinlockGuard g(global_lock_);
    for (const auto& unit : units_) {
        if (unit.active) Log::print_ln("Unit %u (Realm %u): name=%s", unit.id, unit.rid, unit.name);
    }
}

isize UnitManager::get_status(void* manager_ref, void* buffer, const usize size, usize) {
    if (!manager_ref || !buffer || size < sizeof(unit_info_t)) return -EINVAL;

    const auto u = static_cast<Unit*>(manager_ref);
    unit_info_t status;

    status.id = u->id;
    status.realm_id = u->rid;
    status.state = static_cast<u8>(u->state);
    status.priority = u->priority;
    status.cpu_id = u->cpu_id;
    status.exit_code = u->exit_code;
    status.handle_count = u->handle_count;

    status.kernel_stack_start = virt_raw(u->context.stack);
    status.kernel_stack_end = virt_raw(u->context.stack_top);
    status.user_stack_start = virt_raw(u->context.user_stack);
    status.user_stack_end = virt_raw(u->context.user_stack_top);

    memcpy(buffer, &status, sizeof(unit_info_t));
    return sizeof(unit_info_t);
}

void UnitManager::setup_kernel_unit_stack(Unit* u) {
    uptr sp_val = virt_raw(u->context.stack_top);
    sp_val = (sp_val & ~0xF) - 8;
    auto* sp = reinterpret_cast<uptr*>(sp_val);

    // Setup stack for context switching
    *(--sp) = reinterpret_cast<uptr>(kernel::scheduling::manager::unit_trampoline);  // Return RIP
    *(--sp) = 0x202;                                                                      // RFLAGS
    *(--sp) = 0;                                                                          // R15
    *(--sp) = 0;                                                                          // R14
    *(--sp) = 0;                                                                          // R13
    *(--sp) = 0;                                                                          // R12
    *(--sp) = 0;                                                                          // RBX
    *(--sp) = virt_raw(u->context.stack_pointer);                                         // RBP

    u->context.stack_pointer = virt_from_raw(reinterpret_cast<uptr>(sp));
}

void UnitManager::setup_user_unit_stack(Unit* u) {
    uptr sp_val = virt_raw(u->context.stack_top);
    sp_val &= ~0xF;
    sp_val -= 8;
    auto* sp = reinterpret_cast<uptr*>(sp_val);

    *(--sp) = 0x1b;
    *(--sp) = virt_raw(u->context.user_stack_pointer);
    *(--sp) = 0x202;
    *(--sp) = 0x23;
    *(--sp) = reinterpret_cast<uptr>(u->context.entry);

    u->context.stack_pointer = virt_from_raw(reinterpret_cast<uptr>(sp));
}
