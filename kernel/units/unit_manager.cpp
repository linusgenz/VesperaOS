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

#include <filesystem/realmfs.h>
#include <realm/address_space.h>
#include <realm/handle_table.h>
#include <uapi/vespera/dev/unit_info.h>
#include <vespera/log.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/realm/user_stack_allocator.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>
#include <vespera/unit/unit_manager.h>
#include <vespera/unit_config.h>

#include "unit.h"

Unit UnitManager::units_[MAX_UNITS];
Spinlock UnitManager::global_lock_;
UnitId UnitManager::next_id_ = 1;
bool UnitManager::initialized_ = false;

// Segment selector constants (GDT layout)

static constexpr u16 KERNEL_CS = 0x08;
static constexpr u16 KERNEL_DS = 0x10;
static constexpr u16 USER_CS = 0x23;  // ring-3 code segment
static constexpr u16 USER_SS = 0x1B;  // ring-3 data segment
static constexpr u64 INITIAL_RFLAGS = 0x202ULL;

void UnitManager::initialize() {
    global_lock_.init("unit_manager_lock");

    for (auto& u : units_) {
        u.active = false;
        u.id = 0;
        u.rid = 0;
        u.next = nullptr;
    }
    next_id_ = 1;
    initialized_ = true;
}

bool UnitManager::is_initialized() {
    return initialized_;
}

UnitId UnitManager::allocate_id() {
    return next_id_++;
}

Unit* UnitManager::get(const UnitId id) {
    SpinlockGuard g(global_lock_);
    for (auto& u : units_) {
        if (u.active && u.id == id) return &u;
    }
    return nullptr;
}

void UnitManager::list() {
    SpinlockGuard g(global_lock_);
    for (const auto& u : units_) {
        if (u.active) Log::print_ln("Unit %u (Realm %u): name=%s", u.id, u.rid, u.name);
    }
}

Result<usize> UnitManager::get_status(void* manager_ref, void* buffer, usize size, usize /*offset*/) {
    if (!manager_ref || !buffer || size < sizeof(unit_info_t)) return Error::Inval;

    const auto* u = static_cast<Unit*>(manager_ref);
    unit_info_t status{};

    status.id = u->id;
    status.realm_id = u->rid;
    status.state = static_cast<u8>(u->state);
    status.priority = u->priority;
    status.cpu_id = u->cpu_id;
    status.exit_code = u->exit_code;
    status.handle_count = u->handle_count();
    status.kernel_stack_start = virt_raw(u->context.stack);
    status.kernel_stack_end = virt_raw(u->context.stack_top);
    status.user_stack_start = virt_raw(u->context.user_stack);
    status.user_stack_end = virt_raw(u->context.user_stack_top);

    memcpy(buffer, &status, sizeof(unit_info_t));
    return Result<usize>::ok(sizeof(unit_info_t));
}

Unit* UnitManager::create(const RealmId realm_id, const unit_entry_t entry, void* arg, const UnitConfig* cfg) {
    if (!cfg || !entry) return nullptr;

    Realm* realm = RealmManager::get(realm_id);
    if (!realm || !realm->active) return nullptr;

    SpinlockGuard g(global_lock_);

    Unit* u = alloc_slot(realm, cfg->name, entry, arg, cfg);
    if (!u) return nullptr;

    if (!setup_stacks(u, realm, cfg)) {
        u->active = false;
        return nullptr;
    }

    setup_context(u, cfg);

    attach_initial_handles(u, realm, cfg);

    register_unit(u, realm, cfg);

    return u;
}

Unit* UnitManager::alloc_slot(
    Realm* realm, const char* name, const unit_entry_t entry, void* arg, const UnitConfig* cfg
) {
    for (auto& slot : units_) {
        if (slot.active) continue;

        Unit* u = &slot;
        memset(u, 0, sizeof(*u));
        new (u) Unit();

        u->id = allocate_id();
        u->rid = realm->id;
        u->parent = realm;  // already validated by caller
        u->name = strdup(name ? name : "");
        u->active = true;
        u->priority = cfg->priority;
        u->cpu_id = cfg->cpu_id;
        u->is_idle = cfg->is_idle;
        u->is_user = cfg->is_user;
        u->is_main_unit = cfg->is_main_unit;
        u->is_kernel = !cfg->is_user;
        u->context.entry = entry;
        u->context.arg = arg;

        return u;
    }

    Log::warning("UnitManager::create: unit pool exhausted");
    return nullptr;
}

static void* user_vaddr_to_hhdm(const Unit* u, const uptr addr) {
    const uptr offset = addr - virt_raw(u->context.user_stack_virt_base);
    if (offset >= u->context.user_stack_size) return nullptr;
    return virt_ptr(virt_add(u->context.user_stack, offset));
}

static void write_user_ptr(const Unit* u, const uptr addr, const uptr val) {
    void* hhdm = user_vaddr_to_hhdm(u, addr);
    if (hhdm) *static_cast<uptr*>(hhdm) = val;
}

static void memcpy_to_user(const Unit* u, void* user_dest, const void* src, const usize len) {
    void* hhdm = user_vaddr_to_hhdm(u, reinterpret_cast<uptr>(user_dest));
    if (hhdm) memcpy(hhdm, src, len);
}

bool UnitManager::setup_stacks(Unit* u, Realm* realm, const UnitConfig* cfg) {
    const u64 stack_size = cfg->stack_size ? cfg->stack_size : DEFAULT_UNIT_STACK_SIZE;

    if (u->is_user) {
        const u64 user_stack_size = cfg->user_stack_size ? cfg->user_stack_size : stack_size;

        UserStackAllocator::StackSlot slot{};
        if (!realm->address_space->stack_alloc().alloc(slot)) {
            Log::warning("UnitManager: no user stack slots in realm %u", u->rid);
            return false;
        }
        u->user_stack_slot = slot.index;

        const usize pages = (user_stack_size + 0xFFF) / 0x1000;
        const phys_addr_t phys = kernel::memory::request_pages_phys(pages);
        if (phys_null(phys)) {
            realm->address_space->stack_alloc().free(slot.index);
            return false;
        }

        const virt_addr_t hhdm = phys_to_virt(phys);
        memset(virt_ptr(hhdm), 0, user_stack_size);

        realm->address_space->page_table()->map_range(
            slot.virt_base,
            phys,
            user_stack_size,
            (1ULL << PtFlag::Present) | (1ULL << PtFlag::ReadWrite) | (1ULL << PtFlag::UserSuper)
        );

        u->context.user_stack = hhdm;
        u->context.user_stack_phys = phys;
        u->context.user_stack_size = user_stack_size;
        u->context.user_stack_virt_base = slot.virt_base;
        u->context.user_stack_top = slot.virt_top;
        u->context.user_stack_pointer = slot.virt_top;
    }

    u->context.stack = kernel::memory::request_pages((stack_size + 0xFFF) / 0x1000);
    if (virt_null(u->context.stack)) {
        if (u->is_user) {
            kernel::memory::free_pages_phys(u->context.user_stack_phys, (u->context.user_stack_size + 0xFFF) / 0x1000);
            realm->address_space->stack_alloc().free(u->user_stack_slot);
        }
        return false;
    }

    memset(u->context.stack, 0, stack_size);
    u->context.stack_size = stack_size;
    u->context.stack_top = virt_add(u->context.stack, stack_size);
    u->context.stack_pointer = u->context.stack_top;

    fpu_init_state(&u->context.fpu_ctx);
    return true;
}

extern "C" [[noreturn]] void unit_trampoline();
static void init_kernel_cpu_context(Unit* u) {
    uptr rsp = virt_raw(u->context.stack_top);
    rsp &= ~0xFULL;
    rsp -= 8;

    auto& ctx = u->context.cpu_ctx;
    memset(&ctx, 0, sizeof(ctx));

    ctx.rip = reinterpret_cast<u64>(unit_trampoline);
    ctx.cs = KERNEL_CS;
    ctx.rflags = INITIAL_RFLAGS;
    ctx.rsp = rsp;
    ctx.ss = KERNEL_DS;
    ctx.rbp = rsp;
}

static void init_user_cpu_context(Unit* u) {
    auto& ctx = u->context.cpu_ctx;
    memset(&ctx, 0, sizeof(ctx));

    ctx.cs = USER_CS;
    ctx.ss = USER_SS;
    ctx.rflags = INITIAL_RFLAGS;
    ctx.rsp = virt_raw(u->context.user_stack_pointer);

    if (u->is_main_unit) {
        // Entry point is called directly; arguments land in the normal SysV regs.
        ctx.rip = reinterpret_cast<u64>(u->context.entry);
        ctx.rdi = u->context.regs.rdi;
        ctx.rsi = u->context.regs.rsi;
        ctx.rdx = u->context.regs.rdx;
        ctx.rcx = u->context.regs.rcx;
        ctx.r8 = u->context.regs.r8;
        ctx.r9 = u->context.regs.r9;
    } else {
        // Secondary units start via the per-realm trampoline so that the
        // return value flows into SYSCALL_EXIT automatically.
        ctx.rip = USER_UNIT_TRAMPOLINE_VADDR;
        ctx.rdi = reinterpret_cast<u64>(u->context.arg);    // thread argument
        ctx.rsi = reinterpret_cast<u64>(u->context.entry);  // thread entry
    }
}

void UnitManager::setup_context(Unit* u, const UnitConfig* cfg) {
    if (u->is_user) {
        if (cfg->is_main_unit && (cfg->argv || cfg->envp)) setup_user_args_and_env(u, cfg->argv, cfg->envp);
        init_user_cpu_context(u);
    } else {
        init_kernel_cpu_context(u);
    }
}

void UnitManager::attach_initial_handles(Unit* u, Realm* realm, const UnitConfig* cfg) {
    if (!cfg->initial_handles || cfg->initial_handle_count == 0) return;

    for (u64 i = 0; i < cfg->initial_handle_count; ++i) {
        const HandleId h = cfg->initial_handles[i];
        if (!realm->handle_table->lookup(h)) continue;
        u->attach_handle(h);
        realm->handle_table->acquire(h);
    }
}

void UnitManager::register_unit(Unit* u, Realm* realm, const UnitConfig* cfg) {
    if (!u->is_idle && cfg->auto_schedule) kernel::scheduling::add_unit(u);

    {
        SpinlockGuard rg(realm->lock);
        u->realm_next = realm->unit_list;
        realm->unit_list = u;
        realm->unit_count++;
    }

    SYS_EVENT_UNIT_CREATED(u->id, u->rid);
    RealmFs::register_unit(u->id, u->name, u, realm->name);
}

bool UnitManager::destroy(const UnitId id) {
    SpinlockGuard g(global_lock_);

    for (auto& slot : units_) {
        if (!slot.active || slot.id != id) continue;

        Unit* u = &slot;
        Realm* r = u->parent;

        kernel::scheduling::remove_unit(u);

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

        if (u->name) kernel::memory::free(u->name);

        u->detach_all_handles();

        u->free_vma_list();

        if (!virt_null(u->context.stack)) {
            const usize pages = (u->context.stack_size + 0xFFF) / 0x1000;
            kernel::memory::free_pages(u->context.stack, pages);
        }

        if (u->is_user && !virt_null(u->context.user_stack)) {
            const usize pages = (u->context.user_stack_size + 0xFFF) / 0x1000;
            kernel::memory::unmap_range(u->context.user_stack_virt_base, u->context.user_stack_size);
            kernel::memory::free_pages_phys(u->context.user_stack_phys, pages);

            if (r) r->address_space->stack_alloc().free(u->user_stack_slot);
        }

        SYS_EVENT_UNIT_DESTROYED(u->id, u->rid);
        RealmFs::unregister_unit(id);

        const bool realm_empty = r && r->unit_count == 0;
        const RealmId realm_id = r ? r->id : 0;

        memset(u, 0, sizeof(*u));

        if (realm_empty) RealmManager::destroy(realm_id);

        return true;
    }

    return false;
}

// setup_user_args_and_env — writes argc/argv/envp onto the user stack.
//
// Stack layout after this function (grows downward, addresses decreasing):
//
//   [string data for envp strings]
//   null terminator (uptr)
//   envp[envc-1] ... envp[0]   (uptr pointers into string data above)
//   [string data for argv strings]
//   null terminator (uptr)
//   argv[argc-1] ... argv[0]   (uptr pointers into string data above)
//   argc                        (uptr)
//   <- new RSP (16-byte aligned)
//
// The three SysV argument registers are set accordingly:
//   rdi = argc
//   rsi = argv (pointer to argv[0])
//   rdx = envp (pointer to envp[0])
uptr setup_user_args_and_env(Unit* u, const char** argv, const char** envp) {
    static constexpr usize MAX_ARGV = 16;
    static constexpr usize MAX_ENVP = 16;

    uptr sp = virt_raw(u->context.user_stack_top);

    const char* envp_user[MAX_ENVP]{};
    usize envc = 0;
    while (envp && envp[envc] && envc < MAX_ENVP) envc++;

    for (usize i = envc; i > 0; --i) {
        const usize len = strlen(envp[i - 1]) + 1;
        sp -= len;
        sp &= ~0xFULL;
        memcpy_to_user(u, reinterpret_cast<void*>(sp), envp[i - 1], len);
        envp_user[i - 1] = reinterpret_cast<const char*>(sp);
    }

    sp -= sizeof(uptr);
    write_user_ptr(u, sp, 0);

    for (usize i = envc; i > 0; --i) {
        sp -= sizeof(uptr);
        write_user_ptr(u, sp, reinterpret_cast<uptr>(envp_user[i - 1]));
    }
    const uptr envp_ptr = sp;

    const char* argv_user[MAX_ARGV]{};
    usize argc = 0;
    while (argv && argv[argc] && argc < MAX_ARGV) argc++;

    for (usize i = argc; i > 0; --i) {
        const usize len = strlen(argv[i - 1]) + 1;
        sp -= len;
        sp &= ~0xFULL;
        memcpy_to_user(u, reinterpret_cast<void*>(sp), argv[i - 1], len);
        argv_user[i - 1] = reinterpret_cast<const char*>(sp);
    }

    sp -= sizeof(uptr);
    write_user_ptr(u, sp, 0);

    for (usize i = argc; i > 0; --i) {
        sp -= sizeof(uptr);
        write_user_ptr(u, sp, reinterpret_cast<uptr>(argv_user[i - 1]));
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
