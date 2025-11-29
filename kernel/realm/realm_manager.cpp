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

#include <log.h>
#include "../paging/page_table_manager.h"
#include "../../include/kernel/sync/atomic.h"
#include <kernel/system/system_manager.h>
#include "../units/unit_manager.h"

Realm RealmManager::realms[MAX_REALMS];
spinlock_t RealmManager::global_lock;
RealmID RealmManager::next_id = 1;
atomic_u8_t RealmManager::seq;

void RealmManager::initialize() {
    global_lock.init();
    lock_debug_register(&global_lock, "realm_manager_lock");

    seq.init(0);

    for (auto & realm : realms) {
        realm.active = false;
        realm.id = 0;
        realm.unit_list = nullptr;
        realm.unit_count = 0;
    }
    next_id = 1;
}

Realm* RealmManager::create(const RealmConfig* cfg) {
    if (!cfg) return nullptr;

    spinlock_guard g(global_lock);

    seq.fetch_add(1);   // begin write section (odd)

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
            r->envp = cfg->envp;
            r->init_handle_table();

            if (cfg->is_user) {
                auto* kernel_pml4 = reinterpret_cast<PageTable*>(kernel::memory::get_pagetable_address());

                auto* new_pml4 = static_cast<PageTable*>(kernel::memory::request_page());
                memset(new_pml4, 0, 0x1000);
                new_pml4->entries[0] = kernel_pml4->entries[0];

                r->pml4 = new_pml4;
                r->page_table = new PageTableManager(new_pml4);
            }

            SYS_EVENT_REALM_CREATED(r->id, r->name);
            result = r;
            break;
        }
    }

    seq.fetch_add(1);   // end write section (even)

    return result;
}

Realm* RealmManager::get(const RealmID id) {
    while (true) {
        uint8_t begin = seq.load();
        if (begin & 1)           // Writer aktiv → retry
            continue;

        Realm* result = nullptr;

        for (auto & realm : realms) {
            if (realm.active && realm.id == id) {
                result = &realm;
                break;
            }
        }

        uint8_t end = seq.load();
        if (begin == end)
            return result;
    }
}


bool RealmManager::destroy(const RealmID id) {
    spinlock_guard g(global_lock);

    seq.fetch_add(1);   // writer begin

    bool ok = false;
    for (auto& realm : realms) {
        if (realm.active && realm.id == id) {

            SYS_EVENT_REALM_DESTROYED(realm.id, realm.name);

            Unit* u = realm.unit_list;
            while (u) {
                Unit* next = u->next;
                UnitManager::destroy(u->id);
                u = next;
            }

            realm.unit_list = nullptr;
            realm.unit_count = 0;

            realm.clear_handle_table();

            realm.wait_queue.wake_all();

            realm.active = false;
            realm.id = 0;

            ok = true;
            break;
        }
    }

    seq.fetch_add(1);   // writer end

    return ok;
}

void RealmManager::list() {
    while (true) {
        uint8_t begin = seq.load();
        if (begin & 1)          // Writer aktiv
            continue;

        for (const auto & realm : realms) {
            if (realm.active) {
                Log::PrintLn("Realm %u: name=%s, units=%llu/%llu",
                    realm.id,
                    realm.name,
                    static_cast<uint64_t>(realm.unit_count),
                    static_cast<uint64_t>(realm.max_units));
            }
        }

        uint8_t end = seq.load();
        if (begin == end)       // konsistent gelesen?
            return;
    }
}

