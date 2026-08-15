// realm_manager.cpp
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
#include <realm/handle_table.h>
#include <uapi/vespera/dev/realm_info.h>
#include <units/unit.h>
#include <vespera/log.h>
#include <vespera/realm/realm_config.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/realm/realm_types.h>
#include <vespera/sync/atomic.h>
#include <vespera/system/system_manager.h>
#include <vespera/tty/tty.h>
#include <vespera/unit_config.h>

#include "../paging/page_table_manager.h"
#include "../tty/tty_device.h"
#include "address_space.h"
#include "realm.h"
#include "vespera/ipc/vbus_manager.h"
#include "vespera/sys/syscall_numbers.h"
#include "vespera/unit/unit_manager.h"
#include <uapi/vespera/vbus.h>

Realm RealmManager::realms_[MAX_REALMS];
Spinlock RealmManager::global_lock_;
RealmId RealmManager::next_id_ = 1;
atomic_u8_t RealmManager::seq_;
bool RealmManager::initialized_ = false;

phys_addr_t g_trampoline_page;
static u8* g_trampoline_virt;

static u8 signal_trampoline[] = {
    0x48,
    0xC7,
    0xC0,
    15,
    0x00,
    0x00,
    0x00, // mov rax, 36
    0x0F,
    0x05, // syscall
    0xF4  // hlt
};

static u8 unit_trampoline[] = {
    0xFF,
    0xD6, // call rsi
    0x48,
    0x89,
    0xC7, // mov rdi, rax
    0x48,
    0xC7,
    0xC0,
    SYSCALL_EXIT,
    0x00,
    0x00,
    0x00, // mov rax, SYSCALL_EXIT
    0x0F,
    0x05, // syscall
    0xF4  // hlt
};

void RealmManager::initialize() {
    global_lock_.init("realm_manager_lock");

    seq_.init(0);

    for (auto& realm : realms_) {
        realm.active = false;
        realm.id = 0;
        realm.unit_list = nullptr;
        realm.unit_count = 0;
    }
    next_id_ = 1;
    initialized_ = true;

    g_trampoline_page = kernel::memory::request_page_phys();
    g_trampoline_virt = static_cast<u8*>(virt_ptr(phys_to_virt(g_trampoline_page)));

    memset(g_trampoline_virt, 0, 0x1000);

    memcpy(g_trampoline_virt + kernel::realm::TRAMP_SIGNAL_OFF, signal_trampoline, sizeof(signal_trampoline));

    memcpy(g_trampoline_virt + kernel::realm::TRAMP_UNIT_OFF, unit_trampoline, sizeof(unit_trampoline));
}

bool RealmManager::is_initialized() {
    return initialized_;
}

Realm* RealmManager::create(const RealmConfig* cfg) {
    if (!cfg) return nullptr;

    SpinlockGuard g(global_lock_);

    seq_.fetch_add(1); // begin write section (odd)

    Realm* result = nullptr;

    for (auto& realm : realms_) {
        if (!realm.active) {
            Realm* r = &realm;
            r->id = next_id_++;
            if (cfg->name) {
                strncpy(realm.name, cfg->name, sizeof(realm.name) - 1);
                realm.name[sizeof(realm.name) - 1] = '\0';
            }
            strcpy(r->root_path, "/");
            strcpy(r->cwd_path, "/");
            r->memory_limit = cfg->memory_limit;
            r->max_units = cfg->max_units;
            r->unit_list = nullptr;
            r->unit_count = 0;
            r->active = true;
            r->cred = kernel::security::process_credentials{};
            r->lock.init();
            r->capabilities = cfg->capabilities;
            r->handle_table = new HandleTable();
            if (!r->handle_table) return nullptr;
            r->handle_table->init(r->id);
            r->exited = false;
            r->waiter_present = false;
            r->wait_consumed = false;
            r->teardown_complete = false;

            if (cfg->is_user) {
                auto* as = new kernel::realm::AddressSpace();
                if (!as) return nullptr;

                if (as->init(g_trampoline_page, DEFAULT_UNIT_STACK_SIZE) != SUCCESS_CODE) {
                    delete as;
                    return nullptr;
                }

                r->address_space = as;
            }

            SYS_EVENT_REALM_CREATED(r->id, r->name);
            RealmFs::register_realm(r->id, r->name, r);
            result = r;
            break;
        }
    }

    seq_.fetch_add(1); // end write section (even)
    return result;
}

Realm* RealmManager::get(const RealmId id) {
    while (true) {
        const u8 begin = seq_.load();
        if (begin & 1) // Writer aktiv → retry
            continue;

        Realm* result = nullptr;

        for (auto& realm : realms_) {
            if (realm.active && realm.id == id) {
                result = &realm;
                break;
            }
        }

        if (const u8 end = seq_.load(); begin == end) return result;
    }
}

Realm* RealmManager::find_realm_locked(const RealmId id) {
    return nullptr;
}

void RealmManager::abort(const RealmId id) {
    SpinlockGuard g(global_lock_);
    for (auto& realm : realms_) {
        if (!realm.active || realm.id != id) continue;
        if (realm.address_space) {
            realm.address_space->destroy();
            delete realm.address_space;
            realm.address_space = nullptr;
        }
        realm.handle_table->clear();
        VBusManager::unsubscribe_realm(realm.id);
        RealmFs::unregister_realm(realm.id);
        finalize_locked(realm);
        return;
    }
}

bool RealmManager::destroy(const RealmId id, int exit_code) {
    SpinlockGuard g(global_lock_);

    for (auto& realm : realms_) {
        if (!realm.active || realm.id != id) continue;

        if (realm.controlling_tty) {
            kernel::tty::TTY* tty = realm.controlling_tty->tty;
            if (tty && tty->fg_pgid == realm.pgid) {
                tty->fg_pgid = 0;
            }
        }

        VBusManager::unsubscribe_realm(realm.id);
        VBusManager::cancel_pending_for_realm(realm.id);

        RealmFs::unregister_realm(realm.id);

        if (id != kernel::realm::INIT_REALM_ID) {
            for (auto& child : realms_) {
                if (!child.active || child.parent_id != id) continue;

                if (child.exited) {
                    RealmFs::unregister_realm(child.id);
                    finalize_locked(child);
                    continue;
                }

                child.parent_id = kernel::realm::INIT_REALM_ID;

                vbus_orphaned_t payload{};
                payload.realm_id = child.id;
                payload.old_parent_id = id;
                VBusManager::emit_signal(
                    VBUS_IFACE_PROC, VBUS_SIG_PROC_ORPHANED, &payload, sizeof(payload)
                );
            }
        } else {
            kernel::SystemManager::system_panic("init realm exited", -KENOUNIT);
        }

        const Unit* u = realm.unit_list;
        while (u) {
            const Unit* next = u->realm_next;
            UnitManager::destroy(u->id);
            u = next;
        }

        if (realm.address_space) {
            realm.address_space->destroy();
            delete realm.address_space;
            realm.address_space = nullptr;
        }

        realm.unit_list = nullptr;
        realm.unit_count = 0;
        realm.handle_table->clear();

        realm.exited    = true;
        realm.exit_code = exit_code;

        if (realm.wait_consumed) {
            finalize_locked(realm);
        }

        SYS_EVENT_REALM_DESTROYED(realm.id, realm.name);

        return true;
    }

    return false;
}

void RealmManager::mark_teardown_complete(const RealmId id) {
    SpinlockGuard g(global_lock_);
    for (auto& realm : realms_) {
        if (!realm.active || realm.id != id) continue;
        realm.teardown_complete = true;
        if (realm.wait_consumed) {
            finalize_locked(realm);
        }
        return;
    }
}

void RealmManager::finalize_locked(Realm& realm) {
    seq_.fetch_add(1);

    realm.active = false;
    realm.id = 0;

    seq_.fetch_add(1);
}

void RealmManager::reap(const RealmId id) {
    SpinlockGuard g(global_lock_);

    for (auto& realm : realms_) {
        if (!realm.active || realm.id != id) continue;

        realm.wait_consumed = true;

        if (realm.exited && realm.teardown_complete) {
            finalize_locked(realm);
        }
        return;
    }
}

void RealmManager::signal_pgid(const RealmId pgid, const Signal sig) {
    if (pgid == 0) return;

    while (true) {
        const u8 begin = seq_.load();
        if (begin & 1) continue;

        for (const auto& realm : realms_) {
            if (!realm.active || realm.pgid != pgid) continue;
            Unit* u = realm.unit_list;
            while (u) {
                signal_send(u, sig);
                u = u->realm_next;
            }
        }

        if (const u8 end = seq_.load(); begin == end) return;
    }
}

Result<usize> RealmManager::get_status(void* manager_ref, void* buffer, usize size, usize offset) {
    if (!manager_ref || !buffer || size < sizeof(realm_info)) return Error::Inval;

    const auto* r = static_cast<Realm*>(manager_ref);
    realm_info status{};

    status.id = r->id;
    strncpy(status.name, r->name, sizeof(status.name) - 1);
    status.name[sizeof(status.name) - 1] = '\0';
    status.memory_limit = r->memory_limit;
    status.max_units = r->max_units;
    status.unit_count = r->unit_count;
    status.sched_priority = r->sched_priority;
    status.cpu_time_accumulated = r->cpu_time_accumulated;
    status.capabilities = r->capabilities;
    strncpy(status.cwd_path, r->cwd_path, sizeof(status.cwd_path) - 1);
    status.cwd_path[sizeof(status.cwd_path) - 1] = '\0';

    memcpy(buffer, &status, sizeof(realm_info));
    return Result<usize>::ok(sizeof(realm_info));
}

void RealmManager::list() {
    while (true) {
        const u8 begin = seq_.load();
        if (begin & 1) // Writer aktiv
            continue;

        for (const auto& realm : realms_) {
            if (realm.active) {
                Log::print_ln(
                    "Realm %u: name=%s, units=%llu/%llu",
                    realm.id,
                    realm.name,
                    static_cast<u64>(realm.unit_count),
                    static_cast<u64>(realm.max_units)
                );
            }
        }

        if (const u8 end = seq_.load(); begin == end) return;
    }
}

usize RealmManager::get_active_count() {
    while (true) {
        const u8 begin = seq_.load();
        if (begin & 1)
            continue;

        usize count = 0;
        for (const auto& realm : realms_) {
            if (realm.active) {
                count++;
            }
        }

        if (const u8 end = seq_.load(); begin == end) {
            return count;
        }
    }
}