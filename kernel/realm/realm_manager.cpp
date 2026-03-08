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

#include <uapi/vespera/dev/realm_info.h>
#include <vespera/log.h>
#include <vespera/realm/realm_config.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/sync/atomic.h>
#include <vespera/system/system_manager.h>

#include "../../filesystem/realmfs/realmfs.h"
#include "../paging/page_table_manager.h"
#include "../units/unit_manager.h"

Realm RealmManager::realms_[MAX_REALMS];
Spinlock RealmManager::global_lock_;
RealmId RealmManager::next_id_ = 1;
atomic_u8_t RealmManager::seq_;
bool RealmManager::initialized_ = false;

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
}

bool RealmManager::is_initialized() {
    return initialized_;
}

Realm* RealmManager::create(const RealmConfig* cfg) {
    if (!cfg) return nullptr;

    SpinlockGuard g(global_lock_);

    seq_.fetch_add(1);  // begin write section (odd)

    Realm* result = nullptr;

    for (auto& realm : realms_) {
        if (!realm.active) {
            Realm* r = &realm;
            r->id = next_id_++;
            if (cfg->name) {
                strncpy(realm.name, cfg->name, sizeof(realm.name) - 1);
                realm.name[sizeof(realm.name) - 1] = '\0';
            }
            r->memory_limit = cfg->memory_limit;
            r->max_units = cfg->max_units;
            r->unit_list = nullptr;
            r->unit_count = 0;
            r->active = true;
            r->lock.init();
            r->capabilities = cfg->capabilities;
            r->init_handle_table();

            if (cfg->is_user) {
                phys_addr_t pml4_phys = kernel::memory::request_page_phys();

                auto* new_pml4 = static_cast<PageTable*>(virt_ptr(phys_to_virt(pml4_phys)));
                memset(new_pml4, 0, 0x1000);

                const auto* kernel_pml4 =
                    static_cast<PageTable*>(virt_ptr(phys_to_virt(make_phys(kernel::memory::get_pagetable_address()))));

                for (int i = 256; i < 512; i++) new_pml4->entries[i] = kernel_pml4->entries[i];

                r->pml4_phys = pml4_phys;
                r->pml4 = new_pml4;
                r->page_table = new PageTableManager(reinterpret_cast<PageTable*>(phys_raw(pml4_phys)));
            }

            SYS_EVENT_REALM_CREATED(r->id, r->name);
            RealmFs::register_realm(r->id, r->name, r);
            result = r;
            break;
        }
    }

    seq_.fetch_add(1);  // end write section (even)
    return result;
}

Realm* RealmManager::get(const RealmId id) {
    while (true) {
        u8 begin = seq_.load();
        if (begin & 1)  // Writer aktiv → retry
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

bool RealmManager::destroy(const RealmId id) {
    SpinlockGuard g(global_lock_);

    seq_.fetch_add(1);  // writer begin

    bool ok = false;
    for (auto& realm : realms_) {
        if (realm.active && realm.id == id) {
            SYS_EVENT_REALM_DESTROYED(realm.id, realm.name);
            RealmFs::unregister_realm(realm.id);

            Unit* u = realm.unit_list;
            while (u) {
                Unit* next = u->next;
                UnitManager::destroy(u->id);
                u = next;
            }

            if (!phys_null(realm.pml4_phys)) {
                kernel::memory::free_page_phys(realm.pml4_phys);
                realm.pml4_phys = make_phys(0);
                realm.pml4 = nullptr;
            }
            delete realm.page_table;

            realm.page_table = nullptr;

            realm.unit_list = nullptr;
            realm.unit_count = 0;
            realm.clear_handle_table();
            realm.active = false;
            realm.id = 0;

            ok = true;
            break;
        }
    }

    seq_.fetch_add(1);  // writer end

    return ok;
}

isize RealmManager::get_status(void* manager_ref, void* buffer, usize size, usize offset) {
    if (!manager_ref || !buffer || size < sizeof(realm_info)) return -EINVAL;

    auto* r = static_cast<Realm*>(manager_ref);
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
    return sizeof(realm_info);
}

void RealmManager::list() {
    while (true) {
        u8 begin = seq_.load();
        if (begin & 1)  // Writer aktiv
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
