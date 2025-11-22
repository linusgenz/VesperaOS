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

#include <log.h>
#include <scheduling.h>

#include "../realm/realm_manager.h"
#include "../scheduling/schedule_manager.h"
#include "../system/system_manager.h"

Unit UnitManager::units[MAX_UNITS];
spinlock_t UnitManager::global_lock;
UnitID UnitManager::next_id = 1;

void UnitManager::initialize() {
    global_lock.init();
    lock_debug_register(&global_lock, "unit_manager_lock");
    for (auto &unit: units) {
        unit.active = false;
        unit.id = 0;
        unit.rid = 0;
        unit.next = nullptr;
    }
    next_id = 1;
}

UnitID UnitManager::allocate_id() {
    return next_id++;
}

// Schreibe Pointer direkt in den Userstack
static void write_user_ptr(Unit *u, uintptr_t addr, uintptr_t val) {
    uintptr_t offset = addr - (uintptr_t) u->context.user_stack;
    if (offset + sizeof(uintptr_t) > u->context.user_stack_size) {
        return;
    }
    *(uintptr_t *) (u->context.user_stack + offset) = val;
}

// Kopiere Daten direkt in User-Stack
static void memcpy_to_user(Unit *u, void *dest, const void *src, size_t len) {
    uintptr_t offset = (uintptr_t) dest - (uintptr_t) u->context.user_stack;
    if (offset + len > u->context.user_stack_size) return; // Fehlercheck
    memcpy((uint8_t *) u->context.user_stack + offset, src, len);
}

// TODO integrate
uintptr_t SetupUserArgsAndEnv(Unit *u, const char **argv, const char **envp) {
    uintptr_t sp = (uintptr_t) u->context.user_stack_top;

    const char *argv_user[16];
    const char *envp_user[16];

    // --- envp ---
    size_t envc = 0;
    while (envp && envp[envc]) envc++;

    for (ssize_t i = envc - 1; i >= 0; i--) {
        size_t len = strlen(envp[i]) + 1;
        sp -= len;
        sp &= ~0xF; // 16-byte alignment
        memcpy_to_user(u, (void *) sp, envp[i], len);
        envp_user[i] = (const char *) sp;
    }

    sp -= sizeof(uintptr_t);
    write_user_ptr(u, sp, 0);

    for (ssize_t i = envc - 1; i >= 0; i--) {
        sp -= sizeof(uintptr_t);
        write_user_ptr(u, sp, (uintptr_t) envp_user[i]);
    }
    uintptr_t envp_ptr = sp;

    // --- argv ---
    size_t argc = 0;
    while (argv && argv[argc]) argc++;

    for (ssize_t i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;
        sp -= len;
        sp &= ~0xF;
        memcpy_to_user(u, (void *) sp, argv[i], len);
        argv_user[i] = (const char *) sp;
    }

    sp -= sizeof(uintptr_t);
    write_user_ptr(u, sp, 0);

    for (ssize_t i = argc - 1; i >= 0; i--) {
        sp -= sizeof(uintptr_t);
        write_user_ptr(u, sp, (uintptr_t) argv_user[i]);
    }
    uintptr_t argv_ptr = sp;

    sp -= sizeof(uintptr_t);
    write_user_ptr(u, sp, argc);

    u->context.regs.rdi = argc;
    u->context.regs.rsi = argv_ptr;
    u->context.regs.rdx = envp_ptr;
    u->context.user_stack_pointer = (void *) sp;

    return sp;
}


Unit *UnitManager::create(RealmID realm_id, void *entry_point, void *arg, const UnitConfig *cfg) {
    if (!cfg || !entry_point) return nullptr;

    Realm *realm = RealmManager::get(realm_id);
    if (!realm || !realm->active) return nullptr;

    spinlock_guard g(global_lock);

    for (auto &i: units) {
        if (!i.active) {
            Unit *u = &i;

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

            // Handles
            u->handle_count = cfg->initial_handle_count;

            // ExecutionContext
            u->context.entry = (void(*)(void *)) entry_point;
            u->context.arg = arg;

            u->context.regs.rdx = reinterpret_cast<uint64_t>(realm->envp); // 3rd arg for entry

            uint64_t stack_size = cfg->stack_size ? cfg->stack_size : DEFAULT_UNIT_STACK_SIZE;


            if (u->is_user) {
                uint64_t user_stack_size = cfg->user_stack_size
                                               ? cfg->user_stack_size
                                               : (cfg->stack_size ? cfg->stack_size : DEFAULT_UNIT_STACK_SIZE);
                u->context.user_stack = kernel::memory::request_pages((user_stack_size + 0xFFF) / 0x1000);
                if (!u->context.user_stack) {
                    u->active = false;
                    return nullptr;
                }
                kernel::memory::map_range(u->context.user_stack, u->context.user_stack, user_stack_size,
                                          (1ULL << PT_Flag::UserSuper));
                u->context.user_stack_size = user_stack_size;
                u->context.user_stack_top = (void *) ((uintptr_t) u->context.user_stack + user_stack_size);
                u->context.user_stack_pointer = u->context.user_stack_top;
            }

            u->context.stack = kernel::memory::request_pages((stack_size + 0xFFF) / 0x1000);
            if (!u->context.stack) {
                u->active = false;
                return nullptr;
            }
            kernel::memory::map_range(u->context.stack, u->context.stack, stack_size);
            u->context.stack_size = stack_size;
            u->context.stack_top = (void *) ((uintptr_t) u->context.stack + stack_size);
            u->context.stack_pointer = u->context.stack_top;

            if (u->is_idle || u->is_kernel) {
                setup_kernel_unit_stack(u);
            } else if (u->is_user) {
                setup_user_unit_stack(u);
            }

            if (cfg->initial_handles && cfg->initial_handle_count > 0) {
                for (uint64_t j = 0; j < cfg->initial_handle_count; ++j) {
                    HandleID h = cfg->initial_handles[j];
                    // Validate: handle exists in realm handle table & capability checks can be applied
                    if (const handle_entry_t *he = realm->lookup_handle(h); !he) continue; // skip invalid handle

                    u->attach_handle(h);
                    realm->acquire_handle(h);
                }
            }

            if (!u->is_idle) {
                kernel::scheduling::add_unit(u);
            }

            spinlock_guard rg(realm->lock);
            u->realm_next = realm->unit_list;
            realm->unit_list = u;
            realm->unit_count++;

            SYS_EVENT_UNIT_CREATED(u->id, u->rid);

            return u;
        }
    }

    return nullptr; // kein freier Slot
}

Unit *UnitManager::get(const UnitID id) {
    spinlock_guard g(global_lock);
    for (auto &unit: units) {
        if (unit.active && unit.id == id) {
            return &unit;
        }
    }
    return nullptr;
}

bool UnitManager::destroy(const UnitID id) {
    spinlock_guard g(global_lock);

    for (auto &i: units) {
        if (i.active && i.id == id) {
            Unit *u = &i;

            Realm *r = RealmManager::get(u->rid);
            if (r) {
                spinlock_guard rg(r->lock);
                Unit **prev = &r->unit_list;
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

            if (u->context.stack) {
                size_t pages = (u->context.stack_size + 0xFFF) / 0x1000;
                kernel::memory::unmap_range(u->context.stack, u->context.stack_size);
                kernel::memory::free_pages(u->context.stack, pages);
                u->context.stack = nullptr;
            }

            if (u->is_user && u->context.user_stack) {
                size_t user_pages = (u->context.user_stack_size + 0xFFF) / 0x1000;
                kernel::memory::unmap_range(u->context.user_stack, u->context.user_stack_size);
                kernel::memory::free_pages(u->context.user_stack, user_pages);
                u->context.user_stack = nullptr;
            }

            kernel::scheduling::remove_unit(u);

            SYS_EVENT_UNIT_DESTROYED(u->id, u->rid);

            u->active = false;
            u->id = 0;
            u->rid = 0;
            u->name = nullptr;
            u->state = UNIT_NEW;
            u->priority = 0;
            u->cpu_id = 0;
            u->exit_code = 0;
            u->is_user = false;
            u->is_kernel = false;
            u->is_idle = false;
            u->context = {};
            u->sleep_context = {};
            u->next = nullptr;
            u->realm_next = nullptr;

            return true;
        }
    }

    return false;
}

void UnitManager::list() {
    spinlock_guard g(global_lock);
    for (const auto &unit: units) {
        if (unit.active) {
            Log::PrintLn("Unit %u (Realm %u): name=%s",
                         unit.id,
                         unit.rid,
                         unit.name);
        }
    }
}


void UnitManager::setup_kernel_unit_stack(Unit *u) {
    auto *sp = static_cast<uintptr_t *>(u->context.stack_pointer);

    // Setup stack for context switching
    *(--sp) = reinterpret_cast<uintptr_t>(kernel::scheduling::manager::unit_trampoline); // Return RIP
    *(--sp) = 0x202; // RFLAGS
    *(--sp) = 0; // R15
    *(--sp) = 0; // R14
    *(--sp) = 0; // R13
    *(--sp) = 0; // R12
    *(--sp) = 0; // RBX
    *(--sp) = 0; // RBP

    u->context.stack_pointer = sp;
}

void UnitManager::setup_user_unit_stack(Unit *u) {
    auto *sp = static_cast<uintptr_t *>(u->context.stack_pointer);

    *(--sp) = 0x23;
    *(--sp) = reinterpret_cast<uintptr_t>(u->context.user_stack_top);
    *(--sp) = 0x202;
    *(--sp) = 0x1B;
    *(--sp) = reinterpret_cast<uintptr_t>(u->context.entry);

    u->context.stack_pointer = sp;
}
