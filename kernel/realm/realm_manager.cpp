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

#include <kernel/realm/realm_manager.h>
#include <kernel/system/system_manager.h>
#include <log.h>

#include "../../filesystem/realmfs/realmfs.h"
#include "../../include/kernel/sync/atomic.h"
#include "../paging/page_table_manager.h"
#include "../units/unit_manager.h"
#include "dev/realm_info.h"

Realm RealmManager::realms[MAX_REALMS];
spinlock_t RealmManager::global_lock;
RealmID RealmManager::next_id = 1;
atomic_u8_t RealmManager::seq;
bool RealmManager::initialized = false;

void RealmManager::initialize() {
    global_lock.init("realm_manager_lock");

    seq.init(0);

    for (auto& realm : realms) {
        realm.active = false;
        realm.id = 0;
        realm.unit_list = nullptr;
        realm.unit_count = 0;
    }
    next_id = 1;
    initialized = true;
}

bool RealmManager::is_initialized() {
    return initialized;
}

Realm* RealmManager::create(const RealmConfig* cfg) {
    if (!cfg) return nullptr;

    spinlock_guard g(global_lock);

    seq.fetch_add(1);  // begin write section (odd)

    Realm* result = nullptr;

    for (auto& realm : realms) {
        if (!realm.active) {
            Realm* r = &realm;
            r->id = next_id++;
            r->name = cfg->name;
            r->memory_limit = cfg->memory_limit;
            r->max_units = cfg->max_units;
            r->unit_list = nullptr;
            r->unit_count = 0;
            r->active = true;
            r->lock.init();
            r->capabilities = cfg->capabilities;
            r->init_handle_table();

            if (cfg->is_user) {
                uint64_t pml4_phys = kernel::memory::request_page_phys();

                auto* new_pml4 = static_cast<PageTable*>(phys_to_virt(pml4_phys));
                memset(new_pml4, 0, 0x1000);

                auto* kernel_pml4 = static_cast<PageTable*>(phys_to_virt(kernel::memory::get_pagetable_address()));

                for (int i = 256; i < 512; i++) new_pml4->entries[i] = kernel_pml4->entries[i];

                r->pml4_phys = pml4_phys;
                r->pml4 = new_pml4;
                r->page_table = new PageTableManager(reinterpret_cast<PageTable*>(pml4_phys));
            }

            SYS_EVENT_REALM_CREATED(r->id, r->name);
            RealmFS::register_realm(r->id, r->name, r);
            result = r;
            break;
        }
    }

    seq.fetch_add(1);  // end write section (even)
    return result;
}

Realm* RealmManager::get(const RealmID id) {
    while (true) {
        uint8_t begin = seq.load();
        if (begin & 1)  // Writer aktiv → retry
            continue;

        Realm* result = nullptr;

        for (auto& realm : realms) {
            if (realm.active && realm.id == id) {
                result = &realm;
                break;
            }
        }

        uint8_t end = seq.load();
        if (begin == end) return result;
    }
}

bool RealmManager::destroy(const RealmID id) {
    spinlock_guard g(global_lock);

    seq.fetch_add(1);  // writer begin

    bool ok = false;
    for (auto& realm : realms) {
        if (realm.active && realm.id == id) {
            SYS_EVENT_REALM_DESTROYED(realm.id, realm.name);
            RealmFS::unregister_realm(realm.id);

            Unit* u = realm.unit_list;
            while (u) {
                Unit* next = u->next;
                UnitManager::destroy(u->id);
                u = next;
            }

            if (realm.pml4_phys) {
                kernel::memory::free_page_phys(realm.pml4_phys);
                realm.pml4_phys = 0;
                realm.pml4 = nullptr;
            }
            delete realm.page_table;

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

    seq.fetch_add(1);  // writer end

    return ok;
}

ssize_t RealmManager::get_status(void* manager_ref, void* buffer, size_t size, size_t offset) {
    if (!manager_ref || !buffer || size < sizeof(realm_info_t)) return -EINVAL;

    auto* r = static_cast<Realm*>(manager_ref);
    realm_info_t status{};

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

    memcpy(buffer, &status, sizeof(realm_info_t));
    return sizeof(realm_info_t);
}

void RealmManager::list() {
    while (true) {
        uint8_t begin = seq.load();
        if (begin & 1)  // Writer aktiv
            continue;

        for (const auto& realm : realms) {
            if (realm.active) {
                Log::PrintLn(
                    "Realm %u: name=%s, units=%llu/%llu",
                    realm.id,
                    realm.name,
                    static_cast<uint64_t>(realm.unit_count),
                    static_cast<uint64_t>(realm.max_units)
                );
            }
        }

        uint8_t end = seq.load();
        if (begin == end)  // konsistent gelesen?
            return;
    }
}
