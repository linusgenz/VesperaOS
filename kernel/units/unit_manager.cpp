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
#include <kernel/scheduling.h>

#include <kernel/realm/realm_manager.h>
#include "../scheduling/schedule_manager.h"
#include <kernel/system/system_manager.h>

#include "../../filesystem/realmfs/realmfs.h"
#include "dev/unit_info.h"

Unit UnitManager::units[MAX_UNITS];
spinlock_t UnitManager::global_lock;
UnitID UnitManager::next_id = 1;
bool UnitManager::initialized = false;

void UnitManager::initialize()
{
    global_lock.init("unit_manager_lock");
    for (auto& unit : units)
    {
        unit.active = false;
        unit.id = 0;
        unit.rid = 0;
        unit.next = nullptr;
    }
    next_id = 1;
}

bool UnitManager::is_initialized()
{
    return initialized;
}

UnitID UnitManager::allocate_id()
{
    return next_id++;
}

static void write_user_ptr(Unit* u, uintptr_t addr, uintptr_t val)
{
    uintptr_t offset = addr - reinterpret_cast<uintptr_t>(u->context.user_stack);
    if (offset + sizeof(uintptr_t) > u->context.user_stack_size)
    {
        return;
    }
    *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(u->context.user_stack) + offset) = val;
}

static void memcpy_to_user(Unit* u, void* dest, const void* src, size_t len)
{
    uintptr_t offset = reinterpret_cast<uintptr_t>(dest) - reinterpret_cast<uintptr_t>(u->context.user_stack);
    if (offset + len > u->context.user_stack_size) return;
    memcpy(static_cast<uint8_t*>(u->context.user_stack) + offset, src, len);
}

// TODO integrate
uintptr_t SetupUserArgsAndEnv(Unit* u, const char** argv, const char** envp)
{
    auto sp = reinterpret_cast<uintptr_t>(u->context.user_stack_top);

    const char* argv_user[16];
    const char* envp_user[16];

    // --- envp ---
    size_t envc = 0;
    while (envp && envp[envc] && envc < 16) envc++;

    for (size_t i = envc; i > 0; i--)
    {
        const size_t idx = i - 1;
        const size_t len = strlen(envp[idx]) + 1;
        sp -= len;
        sp &= ~0xF;
        memcpy_to_user(u, reinterpret_cast<void*>(sp), envp[idx], len);
        envp_user[idx] = reinterpret_cast<const char*>(sp);
    }


    sp -= sizeof(uintptr_t);
    write_user_ptr(u, sp, 0);

    for (size_t i = envc; i > 0; i--)
    {
        const size_t idx = i - 1;
        sp -= sizeof(uintptr_t);
        write_user_ptr(u, sp, reinterpret_cast<uintptr_t>(envp_user[idx]));
    }
    uintptr_t envp_ptr = sp;

    // --- argv ---
    size_t argc = 0;
    while (argv && argv[argc] && argc < 16) argc++;

    for (size_t i = argc; i > 0; i--)
    {
        const size_t idx = i - 1;
        size_t len = strlen(argv[idx]) + 1;
        sp -= len;
        sp &= ~0xF;
        memcpy_to_user(u, reinterpret_cast<void*>(sp), argv[idx], len);
        argv_user[idx] = reinterpret_cast<const char*>(sp);
    }

    sp -= sizeof(uintptr_t);
    write_user_ptr(u, sp, 0);

    for (size_t i = argc; i > 0; i--)
    {
        const size_t idx = i - 1;
        sp -= sizeof(uintptr_t);
        write_user_ptr(u, sp, reinterpret_cast<uintptr_t>(argv_user[idx]));
    }
    uintptr_t argv_ptr = sp;

    sp -= sizeof(uintptr_t);
    write_user_ptr(u, sp, argc);

    u->context.regs.rdi = argc;
    u->context.regs.rsi = argv_ptr;
    u->context.regs.rdx = envp_ptr;
    u->context.user_stack_pointer = reinterpret_cast<void*>(sp);

    return sp;
}


Unit* UnitManager::create(RealmID realm_id, UnitEntry entry_point, void* arg, const UnitConfig* cfg)
{
    if (!cfg || !entry_point) return nullptr;

    Realm* realm = RealmManager::get(realm_id);
    if (!realm || !realm->active) return nullptr;

    spinlock_guard g(global_lock);

    for (auto& i : units)
    {
        if (!i.active)
        {
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

            // Handles
            u->handle_count = cfg->initial_handle_count;

            // ExecutionContext
            u->context.entry = entry_point;
            u->context.arg = arg;

            uint64_t stack_size = cfg->stack_size ? cfg->stack_size : DEFAULT_UNIT_STACK_SIZE;


            if (u->is_user)
            {
                uint64_t user_stack_size = cfg->user_stack_size
                                               ? cfg->user_stack_size
                                               : (cfg->stack_size ? cfg->stack_size : DEFAULT_UNIT_STACK_SIZE);
                u->context.user_stack = kernel::memory::request_pages((user_stack_size + 0xFFF) / 0x1000);
                if (!u->context.user_stack)
                {
                    u->active = false;
                    return nullptr;
                }
                memset(u->context.user_stack, 0, u->context.user_stack_size);

                kernel::memory::map_range(u->context.user_stack, u->context.user_stack, user_stack_size,
                                          (1ULL << PT_Flag::UserSuper));
                u->context.user_stack_size = user_stack_size;
                u->context.user_stack_top = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(u->context.user_stack) +
                    user_stack_size);
                u->context.user_stack_pointer = u->context.user_stack_top;
            }

            u->context.stack = kernel::memory::request_pages((stack_size + 0xFFF) / 0x1000);
            if (!u->context.stack)
            {
                u->active = false;
                return nullptr;
            }
            memset(u->context.stack, 0, u->context.stack_size);

            kernel::memory::map_range(u->context.stack, u->context.stack, stack_size);
            u->context.stack_size = stack_size;
            u->context.stack_top = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(u->context.stack) + stack_size);
            u->context.stack_pointer = u->context.stack_top;

            if (u->is_idle || u->is_kernel)
            {
                setup_kernel_unit_stack(u);
            }
            else if (u->is_user)
            {
                SetupUserArgsAndEnv(u, cfg->argv, cfg->envp);
                setup_user_unit_stack(u);
            }

            if (cfg->initial_handles && cfg->initial_handle_count > 0)
            {
                for (uint64_t j = 0; j < cfg->initial_handle_count; ++j)
                {
                    HandleID h = cfg->initial_handles[j];
                    // Validate: handle exists in realm handle table & capability checks can be applied
                    if (const handle_entry_t* he = realm->lookup_handle(h); !he) continue; // skip invalid handle

                    u->attach_handle(h);
                    realm->acquire_handle(h);
                }
            }

            if (!u->is_idle && cfg->auto_schedule)
            {
                kernel::scheduling::add_unit(u);
            }

            spinlock_guard rg(realm->lock);
            u->realm_next = realm->unit_list;
            realm->unit_list = u;
            realm->unit_count++;

            SYS_EVENT_UNIT_CREATED(u->id, u->rid);
            RealmFS::register_unit(u->id, u->name, u, realm->name);
            return u;
        }
    }

    return nullptr; // kein freier Slot
}

Unit* UnitManager::get(const UnitID id)
{
    spinlock_guard g(global_lock);
    for (auto& unit : units)
    {
        if (unit.active && unit.id == id)
        {
            return &unit;
        }
    }
    return nullptr;
}

bool UnitManager::destroy(const UnitID id)
{
    spinlock_guard g(global_lock);

    for (auto& i : units)
    {
        if (i.active && i.id == id)
        {
            Unit* u = &i;
            kernel::scheduling::remove_unit(u);

            Realm* r = RealmManager::get(u->rid);
            if (r)
            {
                spinlock_guard rg(r->lock);
                Unit** prev = &r->unit_list;
                while (*prev)
                {
                    if (*prev == u)
                    {
                        *prev = u->realm_next;
                        r->unit_count--;
                        break;
                    }
                    prev = &(*prev)->realm_next;
                }
            }

            u->detach_all_handles();

            if (u->context.stack)
            { // TODO fix and set to page table of unit
                size_t pages = (u->context.stack_size + 0xFFF) / 0x1000;
                kernel::memory::unmap_range(u->context.stack, u->context.stack_size);
                kernel::memory::free_pages(u->context.stack, pages);
                u->context.stack = nullptr;
            }

            if (u->is_user && u->context.user_stack)
            {
                size_t user_pages = (u->context.user_stack_size + 0xFFF) / 0x1000;
                kernel::memory::unmap_range(u->context.user_stack, u->context.user_stack_size);
                kernel::memory::free_pages(u->context.user_stack, user_pages);
                u->context.user_stack = nullptr;
            }

            SYS_EVENT_UNIT_DESTROYED(u->id, u->rid);
            RealmFS::unregister_unit(id);

            if (r && r->unit_count == 0)
            {
                RealmManager::destroy(r->id);
            }

            memset(u, 0, sizeof(*u));
            memset(&u->context, 0, sizeof(u->context));

            return true;
        }
    }

    return false;
}

void UnitManager::list()
{
    spinlock_guard g(global_lock);
    for (const auto& unit : units)
    {
        if (unit.active)
        {
            Log::PrintLn("Unit %u (Realm %u): name=%s",
                         unit.id,
                         unit.rid,
                         unit.name);
        }
    }
}

ssize_t UnitManager::get_status(void* manager_ref, void* buffer, size_t size, size_t offset)
{
    if (!manager_ref || !buffer || size < sizeof(unit_info_t))
        return -EINVAL;

    Unit* u = static_cast<Unit*>(manager_ref);
    unit_info_t status;

    status.id = u->id;
    status.realm_id = u->rid;
    status.state = static_cast<uint8_t>(u->state);
    status.priority = u->priority;
    status.cpu_id = u->cpu_id;
    status.exit_code = u->exit_code;
    status.handle_count = u->handle_count;

    status.kernel_stack_start = reinterpret_cast<uint64_t>(u->context.stack);
    status.kernel_stack_end = reinterpret_cast<uint64_t>(u->context.stack_top);
    status.user_stack_start = reinterpret_cast<uint64_t>(u->context.user_stack);
    status.user_stack_end = reinterpret_cast<uint64_t>(u->context.user_stack_top);

    memcpy(buffer, &status, sizeof(unit_info_t));
    return sizeof(unit_info_t);
}


void UnitManager::setup_kernel_unit_stack(Unit* u)
{
    auto sp_val = reinterpret_cast<uintptr_t>(u->context.stack_top);

    sp_val = (sp_val & ~0xF) - 8; // we have to do this, so when we enter the actual unit, the stack is 16 byte aligned
    auto* sp = reinterpret_cast<uintptr_t*>(sp_val);

    // Setup stack for context switching
    *(--sp) = reinterpret_cast<uintptr_t>(kernel::scheduling::manager::unit_trampoline); // Return RIP
    *(--sp) = 0x202; // RFLAGS
    *(--sp) = 0; // R15
    *(--sp) = 0; // R14
    *(--sp) = 0; // R13
    *(--sp) = 0; // R12
    *(--sp) = 0; // RBX
    *(--sp) = reinterpret_cast<uintptr_t>(u->context.stack_pointer); // RBP

    u->context.stack_pointer = sp;
}

void UnitManager::setup_user_unit_stack(Unit* u)
{
    auto sp_val = reinterpret_cast<uintptr_t>(u->context.stack_top);

    sp_val &= ~0xF;
    sp_val -= 8;
    auto* sp = reinterpret_cast<uintptr_t*>(sp_val);

    *(--sp) = 0x1b;
    *(--sp) = reinterpret_cast<uintptr_t>(u->context.user_stack_pointer);
    *(--sp) = 0x202;
    *(--sp) = 0x23;
    *(--sp) = reinterpret_cast<uintptr_t>(u->context.entry);

    u->context.stack_pointer = sp;
}
