// realm_manager.h
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

#ifndef VESPERAOS_REALM_MANAGER_H
#define VESPERAOS_REALM_MANAGER_H

#include <kernel/realm/realm.h>
#include <kernel/sync/atomic.h>
#include <kernel/sync/spinlock.h>
#include <stddef.h>

#include "../../../kernel/types/types.h"

class RealmManager {
   public:
    static void initialize();
    static bool is_initialized();
    static Realm* create(const RealmConfig* cfg);
    static Realm* get(realm_id_t id);
    static bool destroy(realm_id_t id);
    static ssize_t get_status(void* manager_ref, void* buffer, size_t size, size_t offset);
    static void list();

   private:
    static constexpr size_t MAX_REALMS = 64;
    static Realm realms_[MAX_REALMS];
    static Spinlock global_lock_;
    static realm_id_t next_id_;
    static atomic_u8_t seq_;
    static bool initialized_;
};

#endif  // VESPERAOS_REALM_MANAGER_H