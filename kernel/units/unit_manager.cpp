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
#include <realm/realm.h>
#include <uapi/vespera/dev/unit_info.h>
#include <vespera/log.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/realm/realm_types.h>
#include <vespera/realm/user_stack_allocator.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>
#include <vespera/unit/unit_manager.h>
#include <vespera/unit_config.h>

#include "unit.h"
#include "exec/elf.h"

Unit UnitManager::units_[MAX_UNITS];
Spinlock UnitManager::global_lock_;
UnitId UnitManager::next_id_ = 1;
bool UnitManager::initialized_ = false;

// Segment selector constants (GDT layout)

static constexpr u16 KERNEL_CS = 0x08;
static constexpr u16 KERNEL_DS = 0x10;
static constexpr u16 USER_CS = 0x23; // ring-3 code segment
static constexpr u16 USER_SS = 0x1B; // ring-3 data segment
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

    setup_tls_block(u, realm);

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
        new(u) Unit();

        u->id = allocate_id();
        u->rid = realm->id;
        u->parent = realm; // already validated by caller
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
        u->context.user_stack_top = virt_from_raw(virt_raw(slot.virt_base) + user_stack_size);
        u->context.user_stack_pointer = u->context.user_stack_top;
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

uptr align_up(const uptr v, const usize align) {
    return (v + align - 1) & ~(align - 1);
}

bool UnitManager::setup_tls_block(Unit* u, Realm* realm) {
    const TlsTemplate& tmpl = realm->tls_template;
    if (!tmpl.present) return true;  // kein PT_TLS → nichts zu tun

    // TLS Variant II: [tls_data | tcb]
    const usize block_size = align_up(tmpl.mem_size, tmpl.align);
    constexpr usize TCB_SIZE = 16;
    const usize total = block_size + TCB_SIZE;
    const usize pages = align_up(total, PAGE_SIZE) / PAGE_SIZE;

    const phys_addr_t phys = kernel::memory::request_pages_phys(pages);
    if (phys_null(phys)) return false;

    const virt_addr_t hhdm = phys_to_virt(phys);
    memset(virt_ptr(hhdm), 0, pages * PAGE_SIZE);

    // .tdata initialisieren
    if (tmpl.file_size > 0)
        memcpy(virt_ptr(hhdm), tmpl.init_data, tmpl.file_size);
    // .tbss ist schon null durch memset

    // vaddr im Realm-Adressraum vergeben (bump-allocator reicht)
    const uptr tls_vaddr = __atomic_fetch_add(&realm->tls_region_next,
                                               align_up(total, PAGE_SIZE),
                                               __ATOMIC_ACQ_REL);

    constexpr u64 pt_flags = (1ULL << PtFlag::Present)
                            | (1ULL << PtFlag::UserSuper)
                            | (1ULL << PtFlag::ReadWrite);
    realm->address_space->page_table()->map_range(
        virt_from_raw(tls_vaddr), phys, pages * PAGE_SIZE, pt_flags
    );

    const uptr tcb_uaddr = tls_vaddr + block_size;
    auto* tcb = reinterpret_cast<u64*>(virt_raw(virt_add(hhdm, block_size)));
    tcb[0] = tcb_uaddr;  // self-pointer (x86_64 TLS ABI)
    tcb[1] = 0;

    u->context.fs_base = tcb_uaddr;
    u->tls_phys        = phys;
    u->tls_vaddr       = tls_vaddr;
    u->tls_pages       = pages;

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
        ctx.rip = kernel::realm::USER_UNIT_TRAMPOLINE_VADDR;
        ctx.rdi = reinterpret_cast<u64>(u->context.arg);   // thread argument
        ctx.rsi = reinterpret_cast<u64>(u->context.entry); // thread entry
    }
}

void UnitManager::setup_context(Unit* u, const UnitConfig* cfg) {
    if (u->is_user) {
        if (cfg->is_main_unit) {
            const AuxVectorInfo* aux = cfg->has_aux_info ? &cfg->aux_info : nullptr;
            if (cfg->argv || cfg->envp || aux) setup_user_args_and_env(u, cfg->argv, cfg->envp, aux);
        }
        init_user_cpu_context(u);

        if (u->parent && u->parent->address_space) {
            u->cr3 = phys_raw(u->parent->address_space->pml4_phys());
        }
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

    asm volatile("mfence" ::: "memory");

    SYS_EVENT_UNIT_CREATED(u->id, u->rid);
    RealmFs::register_unit(u->id, u->name, u, realm->name);
}

bool UnitManager::destroy(const UnitId id) {
    SpinlockGuard g(global_lock_);

    for (auto& slot : units_) {
        if (!slot.active || slot.id != id) continue;

        Unit* u = &slot;

        kernel::scheduling::remove_unit(u);

        // Den schweren Speicher-Footprint (Stack, TLS, Handles, VMAs) geben
        // wir SOFORT frei, nicht erst beim finalen Slot-Teardown: der
        // Realm/Prozess kann beliebig lange weiterlaufen, bevor jemand
        // join()t, und eine Zombie-Unit soll währenddessen nur noch ihren
        // exit_code vorhalten. release_resources() ist idempotent, falls sie
        // (z.B. durch exit_current()) schon vorher gelaufen ist.
        release_resources(u);

        if (u->joined_no_wait) {
            // join() ist bereits zurückgekehrt (state war Terminated, exit_code
            // wurde dort schon ausgeliefert) — niemand wartet mehr auf diese
            // Unit. Jetzt, wo die Ressourcen frei sind, kann der Slot final
            // geräumt werden, statt sie als Zombie liegen zu lassen.
            finalize_slot(u);
        } else {
            // Eine Unit wird IMMER zum Zombie, unabhängig davon, ob bereits
            // ein Joiner registriert ist. join() kann jederzeit nach dem Exit
            // kommen (Race) — der finale Slot-Teardown wird erst durch
            // join() oder einen erneuten destroy()-Aufruf (z.B. Realm-
            // Teardown) ausgelöst, niemals automatisch hier.
            u->state = UnitState::Zombie;
            u->wait_queue.wake_all();
        }
        return true;
    }

    return false;
}

/**
 * @brief One `Elf64_auxv_t`-style (tag, value) pair written into the aux vector.
 *
 * Mirrors the SysV ABI `Elf64_auxv_t` layout (`a_type` / `a_un.a_val`) so it can be written to
 * the user stack as a flat array of two `uptr`s per entry, terminated by an `AT_NULL` pair.
 */
struct aux_entry {
    uptr type;
    uptr value;
};

/**
 * @brief Writes the AT_NULL-terminated auxiliary vector below the current stack pointer.
 *
 * @param u     Target unit; credentials are read from its parent realm.
 * @param sp    Current (already 16-byte-unaligned-safe) user stack pointer, growing down.
 * @param aux   ELF image parameters used to populate `AT_PHDR`/`AT_PHENT`/`AT_PHNUM`/`AT_BASE`/`AT_ENTRY`.
 *
 * @return The updated stack pointer, positioned just below the `AT_NULL` terminator.
 *
 * @note Must be called after argv/envp have been written and before the final 16-byte stack
 *       alignment, since the dynamic linker walks the aux vector starting right after `envp`'s
 *       null terminator.
 */
static uptr write_aux_vector(const Unit* u, uptr sp, const AuxVectorInfo& aux) {
    const aux_entry entries[] = {
        {AT_PHDR, aux.phdr_vaddr},
        {AT_PHENT, aux.phent},
        {AT_PHNUM, aux.phnum},
        {AT_PAGESZ, PAGE_SIZE},
        {AT_BASE, aux.has_interp ? aux.interp_base : 0},
        {AT_ENTRY, aux.entry_point},
        {AT_UID, u->parent ? u->parent->cred.uid : 0},
        {AT_EUID, u->parent ? u->parent->cred.euid : 0},
        {AT_GID, u->parent ? u->parent->cred.gid : 0},
        {AT_EGID, u->parent ? u->parent->cred.egid : 0},
        {AT_SECURE, 0},
    };
    constexpr usize entry_count = sizeof(entries) / sizeof(entries[0]);

    sp -= sizeof(aux_entry);
    write_user_ptr(u, sp, AT_NULL);
    write_user_ptr(u, sp + sizeof(uptr), AT_NULL);

    for (usize i = entry_count; i > 0; --i) {
        sp -= sizeof(aux_entry);
        write_user_ptr(u, sp, entries[i - 1].type);
        write_user_ptr(u, sp + sizeof(uptr), entries[i - 1].value);
    }

    return sp;
}

// The three SysV argument registers are set accordingly:
//   rdi = argc
//   rsi = argv (pointer to argv[0])
//   rdx = envp (pointer to envp[0])
//
// @param aux  Optional aux vector info. Pass nullptr to skip writing an aux vector entirely
//             (e.g. for non-main units, where no auxv is expected). The dynamic linker, when
//             present, requires this to be non-null for the main unit.
uptr setup_user_args_and_env(Unit* u, const char** argv, const char** envp, const AuxVectorInfo* aux) {
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

    const usize non_aux_words = argc + envc + 3;
    if (non_aux_words % 2 != 0) {
        sp -= sizeof(uptr);
    }

    if (aux) sp = write_aux_vector(u, sp, *aux);

    sp -= sizeof(uptr);
    write_user_ptr(u, sp, 0);

    for (usize i = envc; i > 0; --i) {
        sp -= sizeof(uptr);
        write_user_ptr(u, sp, reinterpret_cast<uptr>(envp_user[i - 1]));
    }
    const uptr envp_ptr = sp;

    sp -= sizeof(uptr);
    write_user_ptr(u, sp, 0);

    for (usize i = argc; i > 0; --i) {
        sp -= sizeof(uptr);
        write_user_ptr(u, sp, reinterpret_cast<uptr>(argv_user[i - 1]));
    }
    const uptr argv_ptr = sp;

    sp -= sizeof(uptr);
    write_user_ptr(u, sp, argc);

    u->context.regs.rdi = argc;
    u->context.regs.rsi = argv_ptr;
    u->context.regs.rdx = envp_ptr;
    u->context.user_stack_pointer = virt_from_raw(sp);

    return sp;
}


void UnitManager::release_resources(Unit* u) {
    u->detach_all_handles();
    u->free_vma_list();

    if (!virt_null(u->context.stack)) {
        const usize pages = (u->context.stack_size + 0xFFF) / 0x1000;
        kernel::memory::free_pages(u->context.stack, pages);
        u->context.stack = {};
    }

    if (u->is_user && u->tls_pages > 0) {
        kernel::memory::unmap_range(virt_from_raw(u->tls_vaddr), u->tls_pages * PAGE_SIZE);
        kernel::memory::free_pages_phys(u->tls_phys, u->tls_pages);
        u->tls_pages = 0;
    }

    if (u->is_user && !virt_null(u->context.user_stack)) {
        const usize pages = (u->context.user_stack_size + 0xFFF) / 0x1000;
        kernel::memory::unmap_range(u->context.user_stack_virt_base, u->context.user_stack_size);
        kernel::memory::free_pages_phys(u->context.user_stack_phys, pages);
        if (Realm* r = u->parent) r->address_space->stack_alloc().free(u->user_stack_slot);
        u->context.user_stack = {};
    }
}

void UnitManager::complete_termination(Unit* u) {
    release_resources(u);

    SpinlockGuard g(global_lock_);

    Realm* r = u->parent;
    bool realm_empty = false;
    RealmId realm_id = 0;

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
        realm_empty = r->unit_count == 0;
        realm_id = r->id;
    }

    if (u->joined_no_wait) {
        finalize_slot(u);
    } else {
        u->state = UnitState::Zombie;
        u->wait_queue.wake_all();
    }

    if (realm_empty) {
        RealmManager::destroy(realm_id, u->exit_code);
        RealmManager::mark_teardown_complete(realm_id);
    }
}

void UnitManager::finalize_slot(Unit* u) {
    if (u->name) kernel::memory::free(u->name);

    SYS_EVENT_UNIT_DESTROYED(u->id, u->rid);
    RealmFs::unregister_unit(u->id);

    memset(u, 0, sizeof(*u));
}

void UnitManager::reap(Unit* u) {
    release_resources(u);
    finalize_slot(u);
}

Result<int> UnitManager::join(const UnitId id) {
    Unit* current = kernel::scheduling::get_current_unit();
    if (!current)
        return Error::Inval;

    {
        SpinlockGuard g(global_lock_);

        Unit* target = nullptr;
        for (auto& slot : units_) {
            if (slot.active && slot.id == id) {
                target = &slot;
                break;
            }
        }

        if (!target)
            return Error::Srch;

        if (target->joiner_id != 0)
            return Error::Inval;

        if (target->state == UnitState::Zombie) {
            const int exit_code = target->exit_code;
            reap(target);
            return Result<int>::ok(exit_code);
        }

        if (target->state == UnitState::Terminated) {
            const int exit_code = target->exit_code;
            target->joiner_id = current->id;
            target->joined_no_wait = true;
            return Result<int>::ok(exit_code);
        }

        target->joiner_id = current->id;
        target->wait_queue.add_wait(current);
    }

    while (true) {
        kernel::scheduling::yield();

        SpinlockGuard g(global_lock_);

        for (auto& slot : units_) {
            if (slot.active && slot.id == id && slot.state == UnitState::Zombie) {
                const int exit_code = slot.exit_code;
                reap(&slot);
                return Result<int>::ok(exit_code);
            }
        }
    }
}