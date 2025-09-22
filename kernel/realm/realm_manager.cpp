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

#include "realm_manager.h"
#include <log.h>

#include "../units/unit_manager.h"

Realm RealmManager::realms[MAX_REALMS];
spinlock_t RealmManager::global_lock;
RealmID RealmManager::next_id = 1;

void RealmManager::initialize() {
    global_lock.init();
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

    // freien Slot suchen
    for (size_t i = 0; i < MAX_REALMS; i++) {
        if (!realms[i].active) {
            Realm* r = &realms[i];
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

            return r;
        }
    }
    return nullptr; // kein Platz
}

Realm* RealmManager::get(const RealmID id) {
    spinlock_guard g(global_lock);
    for (size_t i = 0; i < MAX_REALMS; i++) {
        if (realms[i].active && realms[i].id == id) {
            return &realms[i];
        }
    }
    return nullptr;
}

bool RealmManager::destroy(const RealmID id) {
    spinlock_guard g(global_lock);
    for (auto & realm : realms) {
        if (realm.active && realm.id == id) {
            Unit* u = realm.unit_list;
            while (u) {
                Unit* next = u->next;
                UnitManager::destroy(u->id);
                u = next;
            }
            realm.unit_list = nullptr;
            realm.unit_count = 0;

            realm.clear_handle_table();

            realm.active = false;
            realm.id = 0;
            return true;
        }
    }
    return false;
}


void RealmManager::list() {
    spinlock_guard g(global_lock);
    for (size_t i = 0; i < MAX_REALMS; i++) {
        if (realms[i].active) {
            Log::PrintLn("Realm %u: name=%s, units=%llu/%llu",
                realms[i].id,
                realms[i].name,
                (unsigned long long)realms[i].unit_count,
                (unsigned long long)realms[i].max_units);
        }
    }
}
